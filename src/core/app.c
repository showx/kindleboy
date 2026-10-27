/*
 * app.c — the state machine, the main loop, and the tricky part: deciding when
 * to push pixels to a screen that's much slower than the game running behind it.
 *
 * The whole e-ink dance, in plain terms:
 *   - The Game Boy always runs at its real 60 Hz (we keep time with a running
 *     accumulator). We only bother actually drawing the picture every other
 *     frame — the player can't tell, and it halves the work.
 *   - We only *send* the picture to the screen about 11 times a second (fast
 *     A2 mode), or ~2 in QUALITY mode. Fast presents are fire-and-forget (the
 *     e-ink controller sorts out overlapping updates itself); only quality and
 *     flashing updates wait for a settled panel.
 *   - If nothing on screen changed, we don't refresh at all. And we only bother
 *     redrawing the strip of rows that did change if it's small.
 *   - Cleaning up e-ink ghosting is done by feel, not on a timer: when the
 *     screen goes still we quietly redraw it as a crisp 4-gray image (which also
 *     makes text much nicer to read), and we only do the jarring full-screen
 *     flash when ghosting has really built up, or on a transition like opening
 *     the menu.
 */
#include "app.h"
#include "emu.h"
#include "render.h"
#include "ui.h"
#include "skin.h"
#include "status.h"
#include "overlay.h"
#include "browser.h"
#include "menu.h"
#include "../platform/platform.h"

#include <string.h>
#include <stdio.h>

/* ---- tuning constants ---------------------------------------------------- */
#define GB_FRAME_US        16742      /* 59.73 Hz */
#define PRESENT_FAST_US    90000      /* ~11 fps A2 (submits don't wait on the panel) */
#define INPUT_BOOST_US     150000     /* after a new button press, present ASAP for a bit */
#define PRESENT_QUALITY_US 450000     /* ~2.2 fps GL16 */
#define QUIET_US           700000     /* still-scene threshold */
#define A2_SOFT            120         /* A2 presents before a GL16 promo flashes */
#define QUALITY_FLASH_EVERY 40         /* GL16 presents between GC16 flashes */
#define BAND_SMALL_ROWS    48          /* <= this many GB rows => partial refresh */
#define RESYNC_FRAMES      4           /* accumulator catch-up clamp */
#define FF_EXTRA_FRAMES    2           /* fast-forward = 1 + this per tick (3x) */
#define STATUS_REFRESH_US  30000000ull /* re-present the clock/battery strip */
#define BATT_REFRESH_US    60000000ull /* re-read the battery level */
#define AUTOSAVE_BACKSTOP_US 300000000ull /* force a battery flush if no quiet moment */

typedef enum { APP_BROWSER, APP_PLAYING, APP_MENU, APP_EXIT } app_state_t;

/* ---- app context (single instance) --------------------------------------- */
static plat_info_t   info;
static uint8_t      *canvas;
static int           fbw, fbh;

static config_t     *cfg;
static const char   *cfg_path;

static emu_t         emu;
static bool          emu_loaded;
static render_cfg_t  rcfg;
static render_cfg_t  screen; /* fixed LCD/shell; game scale may be smaller */
static overlay_t     overlay;
static overlay_feedback_t feedback;
static browser_t     browser;
static menu_t        menu;

static app_state_t   state;

/* present scheduler bookkeeping */
static uint64_t last_present_us;
static uint64_t last_activity_us;
static uint64_t last_autosave_us;
static uint64_t last_flash_us;       /* last full cleanup flash */
static int      a2_count;
static int      quality_present_count;
static bool     promoted;
static bool     du_after_promo;      /* purge promo grays with one DU sweep */

/* fast-forward + deghost button state */
static bool     ff_on;
static bool     prev_deghost_touch, prev_ff_touch;

/* present-on-input: a fresh button press opens a short window during which we
 * present as soon as the panel is free instead of waiting out the rate limit,
 * so movement and menus respond a refresh sooner. */
static uint64_t boost_until_us;
static uint8_t  prev_joypad_bits;

/* status bar (clock + battery) */
static int      status_h;
static int      cached_batt = -1;
static uint64_t last_batt_us;
static uint64_t last_status_us;

/* idle auto-pause */
static uint64_t last_input_us;

/* optional audio: is the output sink currently open */
static bool     audio_open;

/* ---- helpers ------------------------------------------------------------- */

static void present_full(plat_refresh_t mode)
{
	plat_present(0, 0, fbw, fbh, mode);
}

/* Re-read the battery no more than once a minute (the lipc/sysfs read isn't
 * free). Call freely. */
static void refresh_battery(uint64_t now)
{
	if (cached_batt < 0 || now - last_batt_us >= BATT_REFRESH_US) {
		cached_batt = plat_battery_percent();
		last_batt_us = now;
	}
}

static void draw_status(void)
{
	bool low = cached_batt >= 0 && cached_batt <= 15;
	status_draw(canvas, fbw, fbh, cached_batt, ff_on, low, false);
}

static void present_status(void)
{
	plat_present(0, 0, fbw, status_h, REFRESH_QUALITY);
}

static void update_feedback(const plat_input_t *in)
{
	if (state == APP_EXIT) return;
	ov_specials_t sp;
	uint8_t bits=overlay_map(&overlay,in,&sp);
	overlay_damage_t damage[OVERLAY_MAX_DAMAGE];
	int n=overlay_feedback_update(&overlay,&feedback,canvas,fbw,fbh,bits,&sp,
		state==APP_PLAYING && ff_on,plat_now_us(),damage);
	for (int i=0;i<n;i++) {
		overlay_damage_t *r=&damage[i];
		plat_present(r->x,r->y,r->w,r->h,REFRESH_INPUT);
	}
}

static void wait_menu_input(void)
{
	plat_input_wait(overlay_feedback_wait_ms(&feedback,plat_now_us(),500));
}

/* Open or close the audio sink to match cfg->audio. Cheap and idempotent; only
 * called on entering play, on the menu toggle, and on leaving play. If opening
 * fails (no player on the device, etc.) we just stay silent — audio never
 * blocks or breaks anything. */
static void audio_sync(void)
{
	if (cfg->audio && !audio_open)
		audio_open = plat_audio_open(cfg->audio_cmd, EMU_AUDIO_RATE, EMU_AUDIO_CHANNELS);
	else if (!cfg->audio && audio_open) {
		plat_audio_close();
		audio_open = false;
	}
}

/* First paint after entering the browser must be a full GC16 flash: on e-ink a
 * non-flashing refresh over whatever was on screen before (the Kindle home
 * screen, a game) leaves ghosted remnants and half-visible buttons. Selection
 * changes afterwards use the gentler non-flashing refresh. */
static bool browser_needs_flash = true;

static void show_browser(void)
{
	browser_draw(&browser, canvas, fbw, fbh, &screen, cached_batt);
	overlay_feedback_draw(&overlay,&feedback,canvas,fbw,fbh);
	/* First paint clears with a full flash; scroll/selection steps use the fast
	 * B/W DU waveform (~230ms) instead of GL16 (~450ms) so navigation is snappy. */
	if (browser_needs_flash) present_full(REFRESH_FLASH);
	else plat_present(screen.dst_x, screen.dst_y, screen.game_w, screen.game_h, REFRESH_BW);
	browser_needs_flash = false;
	plat_wait_refresh();
}

#if !defined(_WIN32)
#include <sys/stat.h>
static void ensure_dir(const char *p) { mkdir(p, 0777); }
#else
static void ensure_dir(const char *p) { (void)p; }
#endif

/* Find a folder that actually has ROMs in it. We look in the configured dir
 * first, then a few obvious spots — including the extension's own folder, since
 * that's a tempting place to drop a .gb (and where at least one person did).
 * If nothing turns up, we leave the list empty pointing at the configured dir,
 * so the on-screen "no ROMs" message names a real path to use. */
static void rescan_roms(void)
{
	if (browser_scan(&browser, cfg->rom_dir) > 0) return;
	static const char *cands[] = {
		"/mnt/us/roms/gb", "/mnt/us/roms",
		"/mnt/us/extensions/kindleboy", "/mnt/us/extensions/kindleboy/roms",
	};
	for (unsigned i = 0; i < sizeof cands / sizeof cands[0]; i++)
		if (browser_scan(&browser, cands[i]) > 0) return;
	browser_scan(&browser, cfg->rom_dir);
}

static void show_menu(void)
{
	overlay_feedback_reset(&feedback,false);
	menu_draw(&menu, canvas, fbw, fbh, &screen, cfg->quality_mode, cfg->audio, cached_batt);
	present_full(REFRESH_FLASH);   /* flash doubles as a deghost */
	plat_wait_refresh();
}

/* Repaint the whole play screen (game rect + control overlay) and flash. Used
 * on entering gameplay and on resuming from the menu. */
static void repaint_play(void)
{
	int rx, ry, rw, rh;
	uint64_t nowp = plat_now_us();
	refresh_battery(nowp);

	skin_draw(canvas, fbw, fbh, &screen);
	draw_status();
	render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, cfg->quality_mode,
		    &rx, &ry, &rw, &rh);
	overlay_draw(&overlay, canvas, fbw, fbh, ff_on);
	overlay_feedback_reset(&feedback,ff_on);
	present_full(REFRESH_FLASH);
	plat_wait_refresh();

	last_present_us = nowp;
	last_activity_us = nowp;
	last_flash_us = nowp;
	last_status_us = nowp;
	last_input_us = nowp;
	a2_count = 0;
	quality_present_count = 0;
	promoted = false;
	du_after_promo = false;
	emu_frame_consumed(&emu);
}

static bool enter_playing(int idx)
{
	char path[600];
	browser_path(&browser, idx, path, sizeof path);

	if (emu_loaded) { emu_unload(&emu); emu_loaded = false; }

	int r = emu_load(&emu, path);
	if (r != EMU_OK) {
		plat_log("app: emu_load(%s) failed %d", path, r);
		ui_fill(canvas, fbw, fbh, 0, 0, fbw, fbh, 0xFF);
		ui_text(canvas, fbw, fbh, 16, fbh / 2, "Failed to load ROM", 3, 0x00);
		present_full(REFRESH_QUALITY);
		plat_wait_refresh();
		plat_sleep_us(1500000);
		return false;
	}
	emu_loaded = true;
	emu.gb.direct.frame_skip = true;   /* rasterise at 30 Hz, emulate at 60 Hz */

	/* Resume where you left off: load the save state if one exists (ignored on
	 * mismatch / absence). The .sav battery file is already loaded by emu_load. */
	if (cfg->resume && emu_state_load(&emu) == EMU_OK)
		plat_log("app: resumed from save state");

	/* Fresh game, fresh control state. */
	ff_on = false;
	prev_deghost_touch = prev_ff_touch = false;
	boost_until_us = 0;
	prev_joypad_bits = 0;

	/* Remember for next launch. */
	strncpy(cfg->last_rom, path, sizeof cfg->last_rom - 1);
	config_save(cfg, cfg_path);

	audio_sync();   /* open the sound pipe if audio is enabled */

	repaint_play();
	state = APP_PLAYING;
	return true;
}

/* The per-frame present decision during gameplay. */
static void play_present(uint64_t now)
{
	bool quality = cfg->quality_mode;
	bool dirty = emu.dirty_min_y <= emu.dirty_max_y;
	uint64_t min_interval = quality ? PRESENT_QUALITY_US : PRESENT_FAST_US;
	int rx, ry, rw, rh;

	if (dirty) {
		last_activity_us = now;
		promoted = false;
		/* Never submit while the panel is still physically refreshing the last
		 * update: a fresh A2 update onto the same rect collides with the one in
		 * flight, the controller re-drives pixels, and that shows up as flicker.
		 * This is a pure clock check (no blocking ioctl), so emulation keeps
		 * running at full speed — we just hold the picture back until the panel
		 * has settled, roughly the panel's own ~8-9fps ceiling. */
		if (plat_refresh_busy())
			return;
		/* Then honour the soft rate limit, unless a recent button press is
		 * boosting us to get a touch on screen a beat sooner. */
		bool boosting = now < boost_until_us;
		if (!boosting && now - last_present_us < min_interval)
			return;

		/* Periodic full cleanup (configurable; 0 = never): a GC16 flash of the
		 * game area PLUS the status strip above it, so A2 residue never builds
		 * up for long and the clock/battery get refreshed for free. */
		if (!quality && cfg->deghost_sec > 0 &&
		    now - last_flash_us >= (uint64_t)cfg->deghost_sec * 1000000ull) {
			draw_status();
			render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, false, &rx, &ry, &rw, &rh);
			plat_present(0, 0, fbw, rcfg.dst_y + rcfg.game_h, REFRESH_FLASH);
			a2_count = 0;
			last_flash_us = now;
			last_status_us = now;
			du_after_promo = false;
			emu_frame_consumed(&emu);
			last_present_us = now;
			return;
		}

		/* Action resumed after a 4-gray still: A2 can't erase gray pixels,
		 * so run one DU sweep of the whole game rect to purge them first. */
		if (!quality && du_after_promo) {
			render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, false, &rx, &ry, &rw, &rh);
			plat_present(rcfg.dst_x, rcfg.dst_y, rcfg.game_w, rcfg.game_h, REFRESH_BW);
			du_after_promo = false;
			emu_frame_consumed(&emu);
			last_present_us = now;
			return;
		}

		int y0 = emu.dirty_min_y, y1 = emu.dirty_max_y;
		int cx0 = emu.dirty_min_x, cx1 = emu.dirty_max_x;   /* capture before consume */
		int band = y1 - y0 + 1;
		if (band <= BAND_SMALL_ROWS)
			render_game(&emu, &rcfg, canvas, fbw, y0, y1, quality, &rx, &ry, &rw, &rh);
		else
			render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, quality, &rx, &ry, &rw, &rh);

		/* Narrow the refresh to the columns that actually changed (dialogs,
		 * HUD counters) — smaller e-ink updates complete faster and ghost less.
		 * Rounded out to 8px; the canvas already holds the full rendered rows. */
		if (cx0 <= cx1 && cx0 >= 0) {
			int sx0 = (rcfg.dst_x + cx0 * rcfg.scale) & ~7;
			int sx1 = (rcfg.dst_x + (cx1 + 1) * rcfg.scale + 7) & ~7;
			if (sx0 < rcfg.dst_x) sx0 = rcfg.dst_x;
			if (sx1 > rcfg.dst_x + rcfg.game_w) sx1 = rcfg.dst_x + rcfg.game_w;
			rx = sx0; rw = sx1 - sx0;
		}

		plat_present(rx, ry, rw, rh, quality ? REFRESH_QUALITY : REFRESH_FAST);
		emu_frame_consumed(&emu);
		last_present_us = now;

		if (quality) {
			if (++quality_present_count >= QUALITY_FLASH_EVERY) {
				quality_present_count = 0;
				plat_present(rcfg.dst_x, rcfg.dst_y, rcfg.game_w, rcfg.game_h, REFRESH_FLASH);
			}
		} else {
			a2_count++;
		}
	} else {
		/* Scene is quiescent — the ideal moment to flush a battery save,
		 * since the fsync stall is hidden while nothing is animating. */
		if (cfg->autosave_sec > 0 && emu.sram_dirty &&
		    now - last_autosave_us >= (uint64_t)cfg->autosave_sec * 1000000ull) {
			emu_sram_flush(&emu);
			last_autosave_us = now;
		}

		/* Promote the FAST frame to a clean 4-gray still once, which both
		 * deghosts and sharpens static text. GL16 is the known-good, non-flashing
		 * waveform for this on every panel; only escalate to a full GC16 flash
		 * once A2 residue has really piled up. */
		if (!quality && !promoted && !plat_refresh_busy() && last_present_us &&
		    now - last_activity_us >= QUIET_US) {
			render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, true, &rx, &ry, &rw, &rh);
			if (a2_count >= A2_SOFT) {
				plat_present(rcfg.dst_x, rcfg.dst_y, rcfg.game_w, rcfg.game_h, REFRESH_FLASH);
				a2_count = 0;
				last_flash_us = now;
			} else {
				plat_present(rcfg.dst_x, rcfg.dst_y, rcfg.game_w, rcfg.game_h, REFRESH_QUALITY);
				a2_count /= 2;
			}
			promoted = true;
			du_after_promo = true;   /* purge the grays when action resumes */
		}
	}
}

static void handle_menu_action(menu_action_t act)
{
	switch (act) {
	case MENU_RESUME:
		state = APP_PLAYING;
		repaint_play();
		break;
	case MENU_SAVE_STATE:
		emu_state_save(&emu);
		show_menu();      /* stay in menu, flash confirms */
		break;
	case MENU_LOAD_STATE:
		if (emu_state_load(&emu) == EMU_OK) {
			state = APP_PLAYING;
			repaint_play();
		} else {
			show_menu();
		}
		break;
	case MENU_TOGGLE_QUALITY:
		cfg->quality_mode = !cfg->quality_mode;
		config_save(cfg, cfg_path);
		show_menu();
		break;
	case MENU_TOGGLE_AUDIO:
		cfg->audio = !cfg->audio;
		config_save(cfg, cfg_path);
		audio_sync();     /* open/close the sink to match, right away */
		show_menu();      /* redraw so the SOUND: label updates */
		break;
	case MENU_DEGHOST:
		state = APP_PLAYING;
		repaint_play();
		break;
	case MENU_QUIT_TO_BROWSER:
		if (cfg->resume) emu_state_save(&emu);
		emu_sram_flush(&emu);
		emu_unload(&emu);
		emu_loaded = false;
		plat_audio_close(); audio_open = false; /* silence when leaving the game */
		rescan_roms();                          /* refresh list */
		browser.touch_prev = true;              /* finger from this tap is still down */
		browser.prev_buttons = 0xFF;
		overlay_feedback_reset(&feedback,false);
		browser_needs_flash = true;             /* clear the game off the panel */
		state = APP_BROWSER;
		show_browser();
		break;
	case MENU_QUIT:
		state = APP_EXIT;
		break;
	default:
		break;
	}
}

/* ---- main loop ----------------------------------------------------------- */

int app_run(config_t *cfg_in, const char *cfg_path_in, const char *autostart_rom)
{
	cfg = cfg_in;
	cfg_path = cfg_path_in;

	if (plat_init(&info) != 0) {
		plat_log("app: plat_init failed");
		return 1;
	}
	canvas = info.canvas;
	fbw = info.fb_w;
	fbh = info.fb_h;

	status_h = status_height(fbw);
	int min_overlay_h = fbh / 3;
	render_layout(&screen, fbw, fbh, status_h, min_overlay_h);
	rcfg = screen;
	if (cfg->scale_override > 0) {
		int s = cfg->scale_override;
		/* Reject an override that wouldn't fit — otherwise render_game would
		 * write past the canvas. Fall back to the auto layout. */
		if (s <= screen.scale) {
			rcfg.scale = s;
			rcfg.game_w = s * GB_W;
			rcfg.game_h = s * GB_H;
			rcfg.dst_x = (fbw - rcfg.game_w) / 2;
			rcfg.dst_y = screen.dst_y + (screen.game_h - rcfg.game_h) / 2;
		} else {
			plat_log("app: scale_override %d does not fit; using auto", s);
		}
	}
	overlay_layout(&overlay, &screen, fbw, fbh);
	overlay_feedback_reset(&feedback,false);

	ensure_dir("/mnt/us/roms");
	ensure_dir("/mnt/us/roms/gb");
	rescan_roms();
	menu_reset(&menu);
	refresh_battery(plat_now_us());   /* populate before the first screen paints */

	if (autostart_rom && autostart_rom[0]) {
		/* Boot straight into a ROM by path (desktop dev convenience). The
		 * browser stays scanned on cfg->rom_dir so "quit to list" works. */
		if (emu_loaded) emu_unload(&emu);
		int r = emu_load(&emu, autostart_rom);
		if (r == EMU_OK) {
			emu_loaded = true;
			emu.gb.direct.frame_skip = true;
			if (cfg->resume) emu_state_load(&emu);
			ff_on = false;
			prev_deghost_touch = prev_ff_touch = false;
			boost_until_us = 0;
			prev_joypad_bits = 0;
			strncpy(cfg->last_rom, autostart_rom, sizeof cfg->last_rom - 1);
			repaint_play();
			state = APP_PLAYING;
		} else {
			plat_log("app: autostart load failed %d", r);
			state = APP_BROWSER;
			show_browser();
		}
	} else {
		state = APP_BROWSER;
		show_browser();
	}

	uint64_t next_tick = plat_now_us();
	last_autosave_us = next_tick;

	while (state != APP_EXIT) {
		plat_input_t in;
		plat_input_poll(&in);
		if (in.quit_requested) break;

		if (state == APP_BROWSER) {
			refresh_battery(plat_now_us());
			bool changed = false;
			int idx = browser_input(&browser, &in, fbw, fbh, &screen, &changed);
			if (idx == BROWSER_QUIT) {
				state = APP_EXIT;
			} else if (idx >= 0) {
				if (!enter_playing(idx)) {
					browser_needs_flash = true;   /* clear the error screen */
					show_browser();
				}
			} else if (idx == BROWSER_REFRESH) {
				browser_needs_flash = true;
				show_browser();
			} else if (changed) {
				show_browser();
			}
			update_feedback(&in);
			wait_menu_input();   /* release effect wakes once, then sleeps */
			continue;
		}

		if (state == APP_MENU) {
			bool changed = false;
			menu_action_t act = menu_input(&menu, &in, fbw, fbh, &screen, &changed);
			if (act != MENU_NONE) handle_menu_action(act);
			else if (changed) { menu_draw(&menu, canvas, fbw, fbh, &screen, cfg->quality_mode, cfg->audio, cached_batt);
					    overlay_feedback_draw(&overlay,&feedback,canvas,fbw,fbh);
					    plat_present(screen.dst_x, screen.dst_y, screen.game_w, screen.game_h, REFRESH_BW); }
			update_feedback(&in);
			wait_menu_input();
			continue;
		}

		/* APP_PLAYING */
		uint64_t now = plat_now_us();
		refresh_battery(now);

		ov_specials_t sp;
		uint8_t joypad = overlay_map(&overlay, &in, &sp);

		/* Any input resets the idle timer. */
		if (in.count > 0 || joypad || in.key_menu ||
		    in.key_up || in.key_down || in.key_enter)
			last_input_us = now;

		bool go_menu = sp.menu || in.key_menu;
		if (cfg->idle_pause_sec > 0 &&
		    now - last_input_us >= (uint64_t)cfg->idle_pause_sec * 1000000ull)
			go_menu = true;   /* auto-pause: park on the menu to save power */

		if (go_menu) {
			emu_sram_flush(&emu);       /* stall hidden by the flash */
			last_autosave_us = now;
			menu_reset(&menu);
			menu.touch_prev = true;     /* finger from this tap is still down */
			state = APP_MENU;
			show_menu();
			update_feedback(&in);
			next_tick = plat_now_us();
			continue;
		}

		/* DEGHOST button: one tap does an immediate full-screen cleanup flash. */
		if (sp.deghost && !prev_deghost_touch) {
			int rx, ry, rw, rh;
			draw_status();
			render_game(&emu, &rcfg, canvas, fbw, 0, GB_H - 1, false, &rx, &ry, &rw, &rh);
			plat_present(0, 0, fbw, rcfg.dst_y + rcfg.game_h, REFRESH_FLASH);
			last_flash_us = now;
			last_status_us = now;
			du_after_promo = false;
			a2_count = 0;
			emu_frame_consumed(&emu);
			last_present_us = now;
		}

		/* Fast-forward toggles independently of its visual press feedback. */
		if (sp.ff && !prev_ff_touch) {
			ff_on = !ff_on;
		}
		prev_deghost_touch = sp.deghost;
		prev_ff_touch      = sp.ff;

		/* A newly-pressed button opens the present-ASAP window. */
		if (joypad & ~prev_joypad_bits)
			boost_until_us = now + INPUT_BOOST_US;
		prev_joypad_bits = joypad;

		emu_run_frame(&emu, joypad);
		if (ff_on) {
			for (int ff = 0; ff < FF_EXTRA_FRAMES; ff++)
				emu_run_frame(&emu, joypad);
		} else if (audio_open) {
			/* One video-frame of sound per emulated frame at normal speed.
			 * Muted during fast-forward (3x would just be noise). */
			int16_t abuf[EMU_AUDIO_MAX_SAMPLES];
			int n = emu_audio_gen(abuf);
			plat_audio_write(abuf, n);
		}

		now = plat_now_us();
		play_present(now);
		update_feedback(&in);

		/* Keep the clock/battery strip current. */
		if (now - last_status_us >= STATUS_REFRESH_US) {
			draw_status();
			present_status();
			last_status_us = now;
		}

		/* Backstop: if we never hit a quiet moment, force a flush eventually so
		 * progress is never left unsaved for long. */
		if (cfg->autosave_sec > 0 && emu.sram_dirty &&
		    now - last_autosave_us >=
			(uint64_t)cfg->autosave_sec * 1000000ull + AUTOSAVE_BACKSTOP_US) {
			emu_sram_flush(&emu);
			last_autosave_us = now;
		}

		/* Fixed-timestep pacing with catch-up clamp. */
		next_tick += GB_FRAME_US;
		now = plat_now_us();
		if (now < next_tick)
			plat_sleep_us(next_tick - now);
		else if (now - next_tick > (uint64_t)RESYNC_FRAMES * GB_FRAME_US)
			next_tick = now;
	}

	if (emu_loaded) {
		if (cfg->resume) emu_state_save(&emu);
		emu_sram_flush(&emu);
		emu_unload(&emu);
	}
	plat_audio_close();
	config_save(cfg, cfg_path);
	plat_shutdown();
	return 0;
}
