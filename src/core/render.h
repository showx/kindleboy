/*
 * render.h — blow the little 160x144 Game Boy picture up to fit the screen,
 * turning its 4 gray shades into something the e-ink panel can show.
 *
 * Two ways to do it:
 *
 *   FAST (the default): dither down to pure black and white with a 2x2 Bayer
 *     pattern, because the fast "A2" refresh can only do 1-bit anyway. The key
 *     detail is that the dither pattern is tied to where a pixel sits on screen,
 *     not to the frame — so anything that isn't moving comes out byte-for-byte
 *     identical every time, which keeps it from shimmering or ghosting. The two
 *     middle grays are dithered at 1/4 and 3/4 black (not the obvious 1/3 and
 *     2/3) to pull them as far apart as possible; at A2 speed that legibility
 *     matters more than being tonally correct.
 *
 *   QUALITY: skip the dither and use 4 real grays {0xFF,0xAA,0x55,0x00} with the
 *     slower, non-flashing GL16 refresh. This is what a still screen gets
 *     promoted to, and what the QUALITY toggle uses throughout.
 */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>
#include "emu.h"

typedef struct {
	int scale;      /* integer pixel scale (KT4: 3, KT5: 6) */
	int dst_x;      /* game rect origin in the canvas */
	int dst_y;
	int game_w;     /* scale*GB_W */
	int game_h;     /* scale*GB_H */
} render_cfg_t;

/* Choose the largest integer scale that fits the width and leaves top_reserve
 * pixels above the game (for the status bar) plus at least min_overlay_h pixels
 * below it (for touch controls). Centers the game horizontally. */
void render_layout(render_cfg_t *cfg, int fb_w, int fb_h, int top_reserve, int min_overlay_h);

/* Render GB rows [src_y0, src_y1] (inclusive) of e->lcd into canvas. When
 * quality is false, uses the Bayer dither; when true, the 4-gray map. Writes
 * the resulting canvas rectangle to *out_x/y/w/h (already clamped to the game
 * rect) for the caller to hand to plat_present. */
void render_game(const emu_t *e, const render_cfg_t *cfg, uint8_t *canvas,
		 int canvas_w, int src_y0, int src_y1, bool quality,
		 int *out_x, int *out_y, int *out_w, int *out_h);

#endif /* RENDER_H */
