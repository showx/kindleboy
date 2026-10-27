/*
 * platform.h — the one line between "code that runs anywhere" and "code that
 * knows about hardware".
 *
 * Everything above this seam (the emulator glue, the renderer, the UI, the main
 * loop) never touches a real screen or a real finger. It just draws into a plain
 * grayscale buffer the platform hands it. Below the seam is exactly one backend:
 * the Kindle (FBInk + touch), the desktop (an SDL2 window), or the fake one the
 * tests use. Since the core paints every pixel itself, the desktop build looks
 * identical to the Kindle — which is how most of this got built and tested with
 * no Kindle in sight.
 *
 * The buffer ("canvas") is 8-bit gray: one byte a pixel, 0x00 black, 0xFF white,
 * top-left origin, rows packed fb_w bytes apart.
 */
#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

/* ---- lifecycle / geometry ------------------------------------------------ */

typedef struct {
	int      fb_w;      /* framebuffer width  in pixels */
	int      fb_h;      /* framebuffer height in pixels */
	uint8_t *canvas;    /* fb_w*fb_h Y8 buffer, owned by platform, drawn by core */
} plat_info_t;

/* Initialise the backend. Fills *out with geometry and the canvas pointer.
 * Returns 0 on success, negative on failure. */
int  plat_init(plat_info_t *out);

/* Release the backend (restores screensaver state on Kindle, closes fb/evdev,
 * destroys the SDL window). Safe to call once after a successful plat_init. */
void plat_shutdown(void);

/* ---- display ------------------------------------------------------------- */

typedef enum {
	REFRESH_FAST = 0,  /* A2   ~120ms 1-bit  — gameplay default            */
	REFRESH_BW,        /* DU   ~250ms        — fallback / slower gameplay  */
	REFRESH_QUALITY,   /* GL16 ~450ms 16-gray non-flashing — quality mode  */
	REFRESH_GRAY4,     /* DU4  ~250ms 4-gray non-flashing — still promotion */
	REFRESH_FLASH      /* GC16 ~550ms full flash — deghost / transitions   */
} plat_refresh_t;

/* Push canvas sub-rect (x,y,w,h) to the panel and schedule an e-ink refresh of
 * it using the given waveform. Coordinates are in canvas/fb pixels. The
 * platform is responsible for any waveform-specific bracketing (e.g. A2 entry
 * flash). Non-blocking: it does not wait for the refresh to complete. */
void plat_present(int x, int y, int w, int h, plat_refresh_t mode);

/* True while the most recently scheduled refresh is still in flight. The main
 * loop uses this to avoid queueing refreshes faster than the panel drains them;
 * this is the natural e-ink frame pacer. On desktop this is always false. */
bool plat_refresh_busy(void);

/* Block until the most recent refresh completes. Used on state transitions
 * (entering a menu, quitting) where a clean, settled panel matters. */
void plat_wait_refresh(void);

/* ---- input --------------------------------------------------------------- */

#define PLAT_MAX_TOUCHES 5

typedef struct {
	int count;                     /* number of active contacts (0..PLAT_MAX_TOUCHES) */
	struct {
		int x, y;              /* contact position in canvas/fb pixels */
		int id;                /* tracking id (stable across a touch) */
	} pts[PLAT_MAX_TOUCHES];

	/* Desktop keyboard shortcut path: a GB joypad bitmask (JOYPAD_* from
	 * peanut_gb.h) OR'd from currently-held keys. Always 0 on Kindle. Lets
	 * the desktop build be driven by the keyboard without a touch overlay. */
	uint8_t direct_buttons;

	/* Desktop-only UI keys, edge-triggered (set for one poll). Ignored on
	 * Kindle, where the same actions come from touch zones. */
	bool key_enter;   /* confirm / select (Return)          */
	bool key_up;      /* browser move up   (ArrowUp)        */
	bool key_down;    /* browser move down (ArrowDown)      */
	bool key_menu;    /* toggle pause menu (Escape / Tab)   */

	/* Set when the user asked to quit (SDL window close, SIGTERM, or a
	 * long-press of the Kindle power button). The app treats this as a
	 * clean-exit request: flush saves, then leave. */
	bool quit_requested;
} plat_input_t;

/* Drain the input queue without blocking and report current state in *out. */
void plat_input_poll(plat_input_t *out);

/* Block up to timeout_ms waiting for input to arrive, then return (the caller
 * still calls plat_input_poll). Lets idle menu/browser screens sleep instead of
 * busy-polling — near-zero CPU while nothing is happening, instant on a touch.
 * A plain sleep is an acceptable implementation. */
void plat_input_wait(int timeout_ms);

/* Current battery charge 0..100, or -1 if unknown. */
int  plat_battery_percent(void);

/* ---- audio (optional) ---------------------------------------------------- */
/* Open a sink for signed-16 native-endian interleaved PCM at rate/channels.
 * `cmd` is a platform hint: on Kindle it's the shell command to pipe raw PCM
 * into (e.g. aplay); other backends may ignore it. Returns true when a sink is
 * ready. On failure returns false and the app simply plays no sound — audio is
 * always best-effort and never fatal. */
bool plat_audio_open(const char *cmd, int rate, int channels);

/* Queue n_samples int16 values (interleaved stereo, so 2 per frame) for output.
 * Best-effort: may drop samples if the sink is backed up, so it never stalls
 * emulation. No-op when no sink is open. */
void plat_audio_write(const int16_t *samples, int n_samples);

/* Close the audio sink. Safe to call when nothing is open. */
void plat_audio_close(void);

/* ---- timing / misc ------------------------------------------------------- */

uint64_t plat_now_us(void);            /* monotonic microseconds */
void     plat_sleep_us(uint64_t us);   /* sleep for the given microseconds */

/* Log a line. Kindle: appended to /mnt/us/kindleboy.log (with size cap). Desktop:
 * written to stderr. printf-style. */
void plat_log(const char *fmt, ...);

#endif /* PLATFORM_H */
