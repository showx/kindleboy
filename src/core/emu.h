/*
 * emu.h — our wrapper around the Game Boy core (Walnut-CGB).
 *
 * Holds one running Game Boy: the ROM (loaded whole into RAM), the cartridge's
 * battery-backed save RAM, and the latest 160x144 frame at one gray byte per
 * pixel. The core hands us the picture one scanline at a time via a callback.
 * We convert each pixel to 8-bit luma (DMG shades and CGB colours both collapse
 * to gray for e-ink), stash the line, and by diffing it against what was there
 * before, remember the range of rows that actually changed since we last drew —
 * the "dirty band". That's the entire dirty-rectangle scheme, and row-level is
 * all e-ink cares about anyway (a refresh costs about the same whether it covers
 * a line or a strip).
 *
 * The core is Walnut-CGB, a CGB-capable rewrite of Peanut-GB with the same API.
 * Its build knobs live in peanut_impl.c: no sound (the Kindle has no speaker),
 * LCD on, high accuracy (the CPU has cycles to spare).
 */
#ifndef EMU_H
#define EMU_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* walnut_cgb.h is a single-header library: struct gb_s and all function
 * prototypes are always emitted, but the implementation is guarded by
 * WALNUT_GB_HEADER_ONLY. Every TU that includes emu.h gets declarations only;
 * the sole file that instantiates the implementation is peanut_impl.c. The
 * config macros (ENABLE_SOUND/ENABLE_LCD/WALNUT_GB_HIGH_LCD_ACCURACY) do not
 * affect the layout of struct gb_s, so this split is ABI-safe. */
#ifndef WALNUT_GB_HEADER_ONLY
#define WALNUT_GB_HEADER_ONLY
#endif
#include "walnut_cgb.h"

#define GB_W 160
#define GB_H 144

/* Result codes for the load/save entry points. 0 == success. */
enum {
	EMU_OK              =  0,
	EMU_ERR_OPEN        = -1,  /* could not open/read the file            */
	EMU_ERR_TOO_BIG     = -2,  /* ROM larger than we allow                */
	EMU_ERR_INIT        = -3,  /* gb_init rejected the cartridge          */
	EMU_ERR_STATE_MAGIC = -4,  /* save-state header mismatch (wrong game / build) */
	EMU_ERR_NOMEM       = -5
};

typedef struct emu_s {
	struct gb_s gb;

	uint8_t *rom;              /* whole ROM file */
	size_t   rom_size;

	uint8_t *cart_ram;         /* battery-backed SRAM (may be NULL / size 0) */
	size_t   cart_ram_size;

	/* Latest rasterised frame: one 8-bit gray (luma) value per pixel, 0x00
	 * black .. 0xFF white. DMG shades map to four fixed grays; CGB colours are
	 * converted to luma. The renderer dithers/quantises this straight to the
	 * panel, so downstream code never needs to know DMG from CGB. */
	uint8_t  lcd[GB_H][GB_W];

	/* Dirty rectangle since the last emu_frame_consumed(): rows in
	 * [dirty_min_y, dirty_max_y] and columns in [dirty_min_x, dirty_max_x]
	 * changed. When dirty_min_y > dirty_max_y the frame is clean. The column
	 * band lets the caller refresh a narrower slice of e-ink (dialogs, HUDs). */
	int      dirty_min_y;
	int      dirty_max_y;
	int      dirty_min_x;
	int      dirty_max_x;

	bool     sram_dirty;       /* cart RAM written since last flush */
	bool     have_frame;       /* at least one frame rasterised since load */

	char     rom_path[512];    /* absolute path to the loaded ROM */
} emu_t;

/* Load a ROM from disk, initialise Peanut-GB, allocate cart RAM, and load the
 * sibling <rom>.sav battery file if present. Returns EMU_OK or an EMU_ERR_*. */
int  emu_load(emu_t *e, const char *rom_path);

/* Flush the battery save (if dirty) and release all buffers. Safe on a
 * zero-initialised or partially-loaded emu_t. */
void emu_unload(emu_t *e);

/* Advance emulation by one frame. joypad_bits is a mask of pressed buttons
 * using the JOYPAD_* defines (bit set == pressed); this function converts to
 * Peanut-GB's active-low convention internally. Rasterisation of the frame is
 * governed by gb.direct.frame_skip (set by the caller). */
void emu_run_frame(emu_t *e, uint8_t joypad_bits);

/* Reset the dirty band to "clean" after the caller has presented it. */
void emu_frame_consumed(emu_t *e);

/* Write <rom>.sav iff sram_dirty, using temp-file + rename + fsync so a yanked
 * USB cable cannot corrupt an existing save. Clears sram_dirty on success.
 * No-op (returns EMU_OK) when there is no cart RAM or nothing is dirty. */
int  emu_sram_flush(emu_t *e);

/* Single-slot save state to/from <rom>.st. The file is a small header
 * (magic/version/sizeof(struct gb_s)/rom checksum) followed by the raw gb_s
 * struct and the cart RAM. On load, the header is validated and all Peanut-GB
 * callback/priv pointers are re-bound to this process (the saved pointer values
 * are meaningless). States are not portable across builds or between the
 * desktop and Kindle binaries; the battery .sav is the portable artifact. */
int  emu_state_save(emu_t *e);
int  emu_state_load(emu_t *e);

/* Fill title (>=17 bytes) with the cartridge's internal title (NUL-terminated). */
void emu_rom_title(emu_t *e, char title[17]);

/* ---- optional audio ------------------------------------------------------ */
/* The APU mirror is always kept current (cheap), but samples are only ever
 * synthesised when the app calls emu_audio_gen(), which it does only while sound
 * is enabled — so audio-off costs essentially nothing. Output is 32768 Hz,
 * stereo, interleaved signed-16 native endian, one video-frame per gen call. */
#define EMU_AUDIO_RATE      32768
#define EMU_AUDIO_CHANNELS  2
/* Max int16 samples emu_audio_gen() writes (AUDIO_SAMPLES_TOTAL ~= 1096). Sized
 * generously so callers can stack-allocate without pulling in the APU header. */
#define EMU_AUDIO_MAX_SAMPLES 1200

/* Re-initialise the APU (called on load and state-load; safe to call anytime). */
void emu_audio_reset(emu_t *e);

/* Synthesise one video-frame of audio into buf (>= EMU_AUDIO_MAX_SAMPLES int16,
 * interleaved stereo). Returns the number of int16 samples written. */
int  emu_audio_gen(int16_t *buf);

#endif /* EMU_H */
