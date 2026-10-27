/*
 * overlay.h — the buttons we draw on the touchscreen so you can actually play.
 *
 * Layout, tuned to feel like a handheld rather than a ported app:
 *   - Left: a 3x3 D-pad (corner cells press two directions, so diagonals work).
 *   - Right: A and B, staggered like a real Game Boy (A high, B low-left).
 *   - Below: recessed SELECT/START pills and speaker slots.
 *   - Top: CLEAN, 3X, MENU utility keys; 3X inverts while active.
 *
 * Controls repaint locally on press/release, with no idle animation.
 * overlay_map combines the buttons under every finger that's down, so holding
 * a direction while tapping A works.
 */
#ifndef OVERLAY_H
#define OVERLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "render.h"
#include "../platform/platform.h"

typedef enum {
	OV_NONE = 0,   /* a plain joypad zone */
	OV_MENU,       /* open the pause menu */
	OV_DEGHOST,    /* full-screen cleanup flash */
	OV_FF          /* toggle fast-forward */
} ov_special_t;

typedef struct {
	int x, y, w, h;
	uint8_t bit;             /* OR of JOYPAD_* bits (0 for specials) */
	ov_special_t special;
	const char *label;
} touch_zone_t;

typedef struct {
	touch_zone_t zones[20];
	int count;
	int area_y;              /* top of the control area (below the game) */
} overlay_t;

/* Which special buttons are currently under a finger (level, not edge —
 * the app edge-detects so a held finger fires once). */
typedef struct {
	bool menu, deghost, ff;
} ov_specials_t;

#define OVERLAY_MAX_DAMAGE 8 /* D-pad, A/B, SELECT/START and three utility keys */
typedef struct { int x, y, w, h; } overlay_damage_t;
typedef struct {
	uint16_t shown, observed, pending;
	uint64_t release_after[11];
	uint64_t next_us;
	bool ff_on;
} overlay_feedback_t;

void overlay_feedback_reset(overlay_feedback_t *f, bool ff_on);
/* Reapply the current visual state after a screen redraw. Does not refresh. */
void overlay_feedback_draw(const overlay_t *o, const overlay_feedback_t *f,
	uint8_t *canvas, int cw, int ch);
/* Coalesce input, latch short taps long enough to see, and draw changed groups.
 * Returned rectangles contain B/W pixels suitable for a fast local refresh.
 * This is visual state only: it never changes the buttons sent to the game. */
int overlay_feedback_update(const overlay_t *o, overlay_feedback_t *f,
	uint8_t *canvas, int cw, int ch, uint8_t buttons, const ov_specials_t *sp,
	bool ff_on, uint64_t now, overlay_damage_t damage[OVERLAY_MAX_DAMAGE]);
int overlay_feedback_wait_ms(const overlay_feedback_t *f, uint64_t now, int idle_ms);

/* Compute zone rectangles from the game layout and framebuffer size. */
void    overlay_layout(overlay_t *o, const render_cfg_t *rc, int fb_w, int fb_h);

/* Draw all the control outlines and labels. ff_on draws the FF button inverted. */
void    overlay_draw(const overlay_t *o, uint8_t *canvas, int cw, int ch, bool ff_on);

/* Redraw just one special button (e.g. FF toggled), inverted or
 * not. Returns its rect via out params so the caller can present exactly it. */
void    overlay_draw_special(const overlay_t *o, uint8_t *canvas, int cw, int ch,
			     ov_special_t which, bool inverted,
			     int *ox, int *oy, int *ow, int *oh);

/* Map current contacts to a JOYPAD_* bitmask (bit set == pressed) and report
 * which special buttons are touched. Folds in in->direct_buttons (desktop). */
uint8_t overlay_map(const overlay_t *o, const plat_input_t *in, ov_specials_t *sp);

#endif /* OVERLAY_H */
