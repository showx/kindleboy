/*
 * emu.c — the Peanut-GB wrapper: loading and unloading ROMs and save RAM,
 * grabbing each scanline as it's drawn (and noting what changed), writing
 * battery saves, and a one-slot save state.
 */
#include "emu.h"
#include "../platform/platform.h"
#include "minigb_apu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

/* Portable "flush this fd to stable storage". The Kindle and WSL desktop builds
 * are POSIX; the Windows path exists only for off-device compile/run testing. */
#if defined(_WIN32)
#  include <io.h>
#  define emu_fsync(fd) _commit(fd)
#else
#  include <unistd.h>
#  define emu_fsync(fd) fsync(fd)
#endif

#define MAX_ROM_BYTES (8u * 1024u * 1024u)   /* largest cart is 8 MiB */
#define STATE_MAGIC   0x54534247u            /* "GBST" little-endian */
#define STATE_VERSION 2u                     /* 2: Walnut-CGB struct (was Peanut-GB) */

/* Peanut-GB's gb_error() must not return (doing so is documented as SIGABRT).
 * We recover by longjmp-ing back to emu_run_frame, which set this buffer before
 * calling gb_run_frame. Only one GB instance ever exists, so a file-scope
 * buffer is sufficient. in_frame guards against firing outside a frame run. */
static jmp_buf s_frame_jmp;
static volatile bool s_in_frame = false;

/* ---- audio (optional) ---------------------------------------------------- */
/* One APU for the single GB instance. With ENABLE_SOUND, the core calls the two
 * globals below on every APU-register access to keep this mirror current — cheap
 * bookkeeping. The expensive part, synthesising samples, only happens when the
 * app actually asks via emu_audio_gen(), which it does only when sound is on. */
static struct minigb_apu_ctx s_apu;

uint8_t audio_read(uint16_t addr)             { return minigb_apu_audio_read(&s_apu, addr); }
void    audio_write(uint16_t addr, uint8_t v) { minigb_apu_audio_write(&s_apu, addr, v); }

void emu_audio_reset(emu_t *e)  { (void)e; minigb_apu_audio_init(&s_apu); }

int emu_audio_gen(int16_t *buf) /* fills EMU_AUDIO_SAMPLES stereo int16, one frame */
{
	minigb_apu_audio_callback(&s_apu, buf);
	return AUDIO_SAMPLES_TOTAL;
}

/* ---- core callbacks ------------------------------------------------------ */

static uint8_t rom_read_cb(struct gb_s *gb, const uint_fast32_t addr)
{
	emu_t *e = gb->direct.priv;
	if (addr < e->rom_size)
		return e->rom[addr];
	return 0xFF;
}

/* Walnut-CGB's dual-fetch path reads 2 and 4 bytes at a time. ROM on disk is
 * little-endian and both our targets (ARM Kindle, x86 desktop) are too, so a
 * memcpy of the raw bytes gives the right value; memcpy also sidesteps
 * alignment/strict-aliasing UB. Out-of-range tails fall back to byte reads. */
static uint16_t rom_read16_cb(struct gb_s *gb, const uint_fast32_t addr)
{
	emu_t *e = gb->direct.priv;
	if (addr + 1 < e->rom_size) {
		uint16_t v;
		memcpy(&v, e->rom + addr, sizeof v);
		return v;
	}
	return (uint16_t)rom_read_cb(gb, addr) |
	       ((uint16_t)rom_read_cb(gb, addr + 1) << 8);
}

static uint32_t rom_read32_cb(struct gb_s *gb, const uint_fast32_t addr)
{
	emu_t *e = gb->direct.priv;
	if (addr + 3 < e->rom_size) {
		uint32_t v;
		memcpy(&v, e->rom + addr, sizeof v);
		return v;
	}
	return (uint32_t)rom_read_cb(gb, addr) |
	       ((uint32_t)rom_read_cb(gb, addr + 1) << 8) |
	       ((uint32_t)rom_read_cb(gb, addr + 2) << 16) |
	       ((uint32_t)rom_read_cb(gb, addr + 3) << 24);
}

static uint8_t cart_ram_read_cb(struct gb_s *gb, const uint_fast32_t addr)
{
	emu_t *e = gb->direct.priv;
	if (e->cart_ram && addr < e->cart_ram_size)
		return e->cart_ram[addr];
	return 0xFF;
}

static void cart_ram_write_cb(struct gb_s *gb, const uint_fast32_t addr,
			      const uint8_t val)
{
	emu_t *e = gb->direct.priv;
	if (e->cart_ram && addr < e->cart_ram_size) {
		if (e->cart_ram[addr] != val) {
			e->cart_ram[addr] = val;
			e->sram_dirty = true;
		}
	}
}

static void error_cb(struct gb_s *gb, const enum gb_error_e err,
		     const uint16_t addr)
{
	(void)gb;
	plat_log("emu: gb_error %d at 0x%04X", (int)err, addr);
	if (s_in_frame)
		longjmp(s_frame_jmp, 1);
	/* Outside a frame run there is nowhere safe to jump; abort is the
	 * documented behaviour. */
	abort();
}

/* Four fixed grays for DMG shades 0..3 (white..black). These are the exact
 * values the renderer used to hardcode, so DMG output is unchanged. */
static const uint8_t dmg_luma[4] = { 0xFF, 0xAA, 0x55, 0x00 };

/* CGB colour -> luma. fixPalette[] holds up to 64 RGB565 (big-endian on the
 * native panel path, but we only need relative luma) entries; the pixel byte is
 * a 6-bit index into it. Recomputed once per frame (at line 0) since the palette
 * can change between frames but not within one. Rec.601-ish weights. */
static uint8_t s_cgb_luma[64];

static void rebuild_cgb_luma(struct gb_s *gb)
{
	for (int i = 0; i < 64; i++) {
		uint16_t c = gb->cgb.fixPalette[i];
		/* fixPalette is stored as RGB565 (byte-swapped for the LCD); undo the
		 * swap, then expand each channel to 8 bits. */
		uint16_t v = (uint16_t)((c >> 8) | (c << 8));
		int r = ((v >> 11) & 0x1F) << 3;
		int g = ((v >> 5)  & 0x3F) << 2;
		int b = ( v        & 0x1F) << 3;
		s_cgb_luma[i] = (uint8_t)((77 * r + 151 * g + 28 * b) >> 8);
	}
}

/* Capture one rasterised scanline as 8-bit luma. In DMG mode pixels[x] bits 1-0
 * are the shade (upper bits are a palette hint we don't need for gray); in CGB
 * mode the low 6 bits index the colour palette. Only rows that actually changed
 * widen the dirty band. */
static void lcd_line_cb(struct gb_s *gb, const uint8_t *pixels,
			const uint_fast8_t line)
{
	emu_t *e = gb->direct.priv;
	uint8_t row[GB_W];
	int x;

	if (line >= GB_H) return;   /* header doc says 0-144 incl.; array is 0-143 */

	if (gb->cgb.cgbMode) {
		if (line == 0) rebuild_cgb_luma(gb);
		for (x = 0; x < GB_W; x++)
			row[x] = s_cgb_luma[pixels[x] & 0x3F];
	} else {
		for (x = 0; x < GB_W; x++)
			row[x] = dmg_luma[pixels[x] & 0x03];
	}

	if (memcmp(row, e->lcd[line], GB_W) != 0) {
		int first = 0, last = GB_W - 1;
		while (first < GB_W && row[first] == e->lcd[line][first]) first++;
		while (last > first && row[last] == e->lcd[line][last]) last--;
		memcpy(e->lcd[line], row, GB_W);
		if ((int)line < e->dirty_min_y) e->dirty_min_y = line;
		if ((int)line > e->dirty_max_y) e->dirty_max_y = line;
		if (first < e->dirty_min_x) e->dirty_min_x = first;
		if (last  > e->dirty_max_x) e->dirty_max_x = last;
	}
	e->have_frame = true;
}

/* Bind every Peanut-GB callback + priv to this process/instance. Called once
 * after gb_init and again after loading a save state (which overwrites the
 * struct, including these pointers, with meaningless saved values). */
static void bind_callbacks(emu_t *e)
{
	e->gb.gb_rom_read       = rom_read_cb;
	e->gb.gb_rom_read_16bit = rom_read16_cb;
	e->gb.gb_rom_read_32bit = rom_read32_cb;
	e->gb.gb_cart_ram_read  = cart_ram_read_cb;
	e->gb.gb_cart_ram_write = cart_ram_write_cb;
	e->gb.gb_error          = error_cb;
	/* We use neither serial nor a boot ROM; NULL these so a restored state
	 * file can never leave an arbitrary pointer here for Peanut-GB to call. */
	e->gb.gb_serial_tx     = NULL;
	e->gb.gb_serial_rx     = NULL;
	e->gb.gb_bootrom_read  = NULL;
	e->gb.direct.priv      = e;
	gb_init_lcd(&e->gb, lcd_line_cb);
}

/* ---- helpers ------------------------------------------------------------- */

/* Replace the extension of rom_path with ext (e.g. ".sav"), into out. */
static void sibling_path(const emu_t *e, const char *ext, char *out, size_t n)
{
	const char *dot = strrchr(e->rom_path, '.');
	size_t base = dot ? (size_t)(dot - e->rom_path) : strlen(e->rom_path);
	if (base >= n) base = n - 1;
	memcpy(out, e->rom_path, base);
	out[base] = '\0';
	strncat(out, ext, n - base - 1);
}

/* Atomic write: temp file in the same directory, fsync the data, then rename
 * over the target. FAT32-safe against a yanked USB cable — a partial write only
 * ever lands in the .tmp, never the real save. Returns 0. */
static int atomic_write(const char *path, const void *data, size_t len)
{
	char tmp[600];
	snprintf(tmp, sizeof tmp, "%s.tmp", path);

	FILE *f = fopen(tmp, "wb");
	if (!f) return EMU_ERR_OPEN;
	if (len && fwrite(data, 1, len, f) != len) { fclose(f); remove(tmp); return EMU_ERR_OPEN; }
	fflush(f);
	emu_fsync(fileno(f));
	fclose(f);

#if defined(_WIN32)
	remove(path);   /* Windows rename() won't overwrite an existing target */
#endif
	if (rename(tmp, path) != 0) { remove(tmp); return EMU_ERR_OPEN; }
	return EMU_OK;
}

/* ---- public API ---------------------------------------------------------- */

int emu_load(emu_t *e, const char *rom_path)
{
	memset(e, 0, sizeof *e);
	e->dirty_min_y = 0;
	e->dirty_max_y = GB_H - 1;   /* first present paints the whole frame */
	e->dirty_min_x = 0;
	e->dirty_max_x = GB_W - 1;
	strncpy(e->rom_path, rom_path, sizeof e->rom_path - 1);

	/* Read the whole ROM. */
	FILE *f = fopen(rom_path, "rb");
	if (!f) return EMU_ERR_OPEN;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || (unsigned long)sz > MAX_ROM_BYTES) { fclose(f); return EMU_ERR_TOO_BIG; }
	e->rom = malloc((size_t)sz);
	if (!e->rom) { fclose(f); return EMU_ERR_NOMEM; }
	if (fread(e->rom, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); emu_unload(e); return EMU_ERR_OPEN; }
	fclose(f);
	e->rom_size = (size_t)sz;

	/* Init the core (needs rom_read to inspect the header). */
	e->gb.direct.priv = e;
	e->gb.gb_rom_read = rom_read_cb;   /* gb_init reads header via this */
	enum gb_init_error_e ie = gb_init(&e->gb, rom_read_cb, rom_read16_cb,
					  rom_read32_cb, cart_ram_read_cb,
					  cart_ram_write_cb, error_cb, e);
	if (ie != GB_INIT_NO_ERROR) { plat_log("emu: gb_init err %d", ie); emu_unload(e); return EMU_ERR_INIT; }
	bind_callbacks(e);
	emu_audio_reset(e);

	/* Allocate battery RAM and load the .sav if present. */
	size_t ram = 0;
	gb_get_save_size_s(&e->gb, &ram);
	e->cart_ram_size = ram;
	if (ram) {
		e->cart_ram = calloc(1, ram);
		if (!e->cart_ram) { emu_unload(e); return EMU_ERR_NOMEM; }
		char sav[600];
		sibling_path(e, ".sav", sav, sizeof sav);
		FILE *sf = fopen(sav, "rb");
		if (sf) {
			fread(e->cart_ram, 1, ram, sf);   /* short read tolerated */
			fclose(sf);
			plat_log("emu: loaded battery save %s", sav);
		}
	}

	e->sram_dirty = false;
	e->have_frame = false;
	plat_log("emu: loaded %s (%ld bytes, %zu ram)", rom_path, sz, ram);
	return EMU_OK;
}

void emu_unload(emu_t *e)
{
	if (!e) return;
	emu_sram_flush(e);
	free(e->rom);       e->rom = NULL;       e->rom_size = 0;
	free(e->cart_ram);  e->cart_ram = NULL;  e->cart_ram_size = 0;
}

void emu_run_frame(emu_t *e, uint8_t joypad_bits)
{
	/* JOYPAD_* bit set == pressed; Peanut-GB wants active-low. */
	e->gb.direct.joypad = (uint8_t)~joypad_bits;

	if (setjmp(s_frame_jmp) == 0) {
		s_in_frame = true;
		gb_run_frame(&e->gb);
	} else {
		plat_log("emu: recovered from gb_error during frame");
	}
	s_in_frame = false;
}

void emu_frame_consumed(emu_t *e)
{
	e->dirty_min_y = GB_H;    /* min > max == clean */
	e->dirty_max_y = -1;
	e->dirty_min_x = GB_W;
	e->dirty_max_x = -1;
}

int emu_sram_flush(emu_t *e)
{
	if (!e || !e->cart_ram || !e->sram_dirty) return EMU_OK;
	char sav[600];
	sibling_path(e, ".sav", sav, sizeof sav);
	int r = atomic_write(sav, e->cart_ram, e->cart_ram_size);
	if (r == EMU_OK) {
		e->sram_dirty = false;
		plat_log("emu: flushed battery save %s", sav);
	} else {
		plat_log("emu: FAILED to flush battery save %s", sav);
	}
	return r;
}

/* Save-state file: header, then raw struct gb_s, then cart RAM. */
struct state_header {
	uint32_t magic;
	uint32_t version;
	uint32_t gb_struct_size;
	uint32_t rom_checksum;   /* sum of ROM bytes, for wrong-game detection */
	uint32_t cart_ram_size;
};

static uint32_t rom_checksum(const emu_t *e)
{
	uint32_t s = 0;
	for (size_t i = 0; i < e->rom_size; i++) s += e->rom[i];
	return s;
}

int emu_state_save(emu_t *e)
{
	char path[600];
	sibling_path(e, ".st", path, sizeof path);

	size_t total = sizeof(struct state_header) + sizeof(struct gb_s) + e->cart_ram_size;
	uint8_t *buf = malloc(total);
	if (!buf) return EMU_ERR_NOMEM;

	struct state_header h = {
		.magic = STATE_MAGIC, .version = STATE_VERSION,
		.gb_struct_size = (uint32_t)sizeof(struct gb_s),
		.rom_checksum = rom_checksum(e),
		.cart_ram_size = (uint32_t)e->cart_ram_size,
	};
	uint8_t *p = buf;
	memcpy(p, &h, sizeof h);                  p += sizeof h;
	memcpy(p, &e->gb, sizeof(struct gb_s));   p += sizeof(struct gb_s);
	if (e->cart_ram_size) memcpy(p, e->cart_ram, e->cart_ram_size);

	int r = atomic_write(path, buf, total);
	free(buf);
	plat_log("emu: state_save %s -> %d", path, r);
	return r;
}

int emu_state_load(emu_t *e)
{
	char path[600];
	sibling_path(e, ".st", path, sizeof path);

	FILE *f = fopen(path, "rb");
	if (!f) return EMU_ERR_OPEN;

	/* Validate the header and stage the payload entirely before touching the
	 * live instance. A truncated or wrong file must never leave e->gb holding
	 * un-rebound (garbage) function pointers, which the app would then call. */
	struct state_header h;
	if (fread(&h, 1, sizeof h, f) != sizeof h) { fclose(f); return EMU_ERR_OPEN; }
	if (h.magic != STATE_MAGIC || h.version != STATE_VERSION ||
	    h.gb_struct_size != sizeof(struct gb_s) ||
	    h.rom_checksum != rom_checksum(e) ||
	    h.cart_ram_size != e->cart_ram_size) {
		plat_log("emu: state_load rejected (magic/size/game mismatch)");
		fclose(f);
		return EMU_ERR_STATE_MAGIC;
	}

	struct gb_s tmp;
	if (fread(&tmp, 1, sizeof tmp, f) != sizeof tmp) { fclose(f); return EMU_ERR_OPEN; }

	uint8_t *tmp_ram = NULL;
	if (e->cart_ram_size) {
		tmp_ram = malloc(e->cart_ram_size);
		if (!tmp_ram) { fclose(f); return EMU_ERR_NOMEM; }
		if (fread(tmp_ram, 1, e->cart_ram_size, f) != e->cart_ram_size) {
			free(tmp_ram); fclose(f); return EMU_ERR_OPEN;
		}
	}
	fclose(f);

	/* Fully validated: commit. */
	memcpy(&e->gb, &tmp, sizeof e->gb);
	if (tmp_ram) { memcpy(e->cart_ram, tmp_ram, e->cart_ram_size); free(tmp_ram); }

	/* Saved pointer values are meaningless in this process — re-bind. */
	bind_callbacks(e);
	emu_audio_reset(e);               /* APU isn't in the saved struct; re-init */
	e->gb.direct.frame_skip = true;   /* gb_init_lcd cleared it; restore policy */
	e->dirty_min_y = 0;               /* force a full repaint of the restored frame */
	e->dirty_max_y = GB_H - 1;
	e->dirty_min_x = 0;
	e->dirty_max_x = GB_W - 1;
	e->sram_dirty = true;             /* cart RAM may differ now; persist it */
	plat_log("emu: state_load %s ok", path);
	return EMU_OK;
}

void emu_rom_title(emu_t *e, char title[17])
{
	/* gb_get_rom_name copies up to 16 title chars then a NUL: 17 bytes. */
	gb_get_rom_name(&e->gb, title);
}
