/*
 * overlay.c — where the on-screen buttons go, how they're drawn, and figuring
 * out which one a finger landed on.
 */
#include "overlay.h"
#include "ui.h"
#include "emu.h"      /* JOYPAD_* */
#include <string.h>

static void add_zone(overlay_t *o, int x, int y, int w, int h,
		     uint8_t bit, ov_special_t special, const char *label)
{
	if (o->count >= (int)(sizeof o->zones / sizeof o->zones[0])) return;
	touch_zone_t *z = &o->zones[o->count++];
	z->x = x; z->y = y; z->w = w; z->h = h;
	z->bit = bit; z->special = special; z->label = label; z->fixed_px = 0;
}

/* Largest text scale that fits `label` inside a w x h box (matches fit_px). */
static int fit_px_wh(const char *label, int w, int h)
{
	int len = (int)strlen(label);
	if (len == 0) return 1;
	int px_w = (w - 6) / (8 * len);
	int px_h = (h - 6) / 8;
	int px = px_w < px_h ? px_w : px_h;
	if (px < 1) px = 1;
	if (px > 3) px = 3;
	return px;
}

void overlay_layout(overlay_t *o, const render_cfg_t *rc, int fb_w, int fb_h)
{
	memset(o, 0, sizeof *o);

	const int pad = 8;
	const int gap = 6;

	/* Compact bottom row: START SELECT MENU SAVE FF. */
	int row_h = fb_h / 18;                    /* ~44px on an 800px panel */
	if (row_h < 36) row_h = 36;
	int row_y = fb_h - pad - row_h;

	/* Main control band between the game and the bottom row. */
	int y0 = rc->dst_y + rc->game_h + pad;
	o->area_y = y0;
	int main_h = row_y - pad - y0;
	if (main_h < 60) main_h = 60;

	/* D-pad: as large as the band allows, capped at ~45% of the width. */
	int dside = main_h;
	int dmax  = (fb_w * 45) / 100;
	if (dside > dmax) dside = dmax;
	int dx = pad;
	int dy = y0 + (main_h - dside) / 2;
	int c = dside / 3;

	add_zone(o, dx,       dy,       c, c, JOYPAD_UP|JOYPAD_LEFT,    OV_NONE, "");
	add_zone(o, dx + c,   dy,       c, c, JOYPAD_UP,                OV_NONE, "^");
	add_zone(o, dx + 2*c, dy,       c, c, JOYPAD_UP|JOYPAD_RIGHT,   OV_NONE, "");
	add_zone(o, dx,       dy + c,   c, c, JOYPAD_LEFT,              OV_NONE, "<");
	add_zone(o, dx + 2*c, dy + c,   c, c, JOYPAD_RIGHT,             OV_NONE, ">");
	add_zone(o, dx,       dy + 2*c, c, c, JOYPAD_DOWN|JOYPAD_LEFT,  OV_NONE, "");
	add_zone(o, dx + c,   dy + 2*c, c, c, JOYPAD_DOWN,              OV_NONE, "v");
	add_zone(o, dx + 2*c, dy + 2*c, c, c, JOYPAD_DOWN|JOYPAD_RIGHT, OV_NONE, "");

	/* A and B, staggered like a Game Boy: A high on the outside, B lower and
	 * inward. Sized like a D-pad cell but a touch bigger. */
	int bsize = (c * 5) / 4;
	int ax = fb_w - pad - bsize;
	int ay = dy + dside / 2 - bsize;
	if (ay < y0) ay = y0;
	int bx = ax - bsize - pad;
	int by = ay + bsize / 2 + pad;
	if (by + bsize > row_y - pad) by = row_y - pad - bsize;
	add_zone(o, ax, ay, bsize, bsize, JOYPAD_A, OV_NONE, "A");
	add_zone(o, bx, by, bsize, bsize, JOYPAD_B, OV_NONE, "B");

	/* Bottom row: five compact buttons across the full width. */
	int bw = (fb_w - 2 * pad - 4 * gap) / 5;
	int first = o->count;
	int x = pad;
	add_zone(o, x, row_y, bw, row_h, JOYPAD_START,  OV_NONE,    "START");   x += bw + gap;
	add_zone(o, x, row_y, bw, row_h, JOYPAD_SELECT, OV_NONE,    "SELECT");  x += bw + gap;
	add_zone(o, x, row_y, bw, row_h, 0,             OV_MENU,    "MENU");    x += bw + gap;
	add_zone(o, x, row_y, bw, row_h, 0,             OV_DEGHOST, "DEGHOST"); x += bw + gap;
	add_zone(o, x, row_y, bw, row_h, 0,             OV_FF,      "FF");

	/* Give the whole bottom row one uniform text scale (the smallest that fits
	 * the longest label) so they read as a matched set, not staggered sizes. */
	int uni = 3;
	for (int i = first; i < o->count; i++) {
		int p = fit_px_wh(o->zones[i].label, o->zones[i].w, o->zones[i].h);
		if (p < uni) uni = p;
	}
	for (int i = first; i < o->count; i++)
		o->zones[i].fixed_px = uni;
}

/* Pick the largest text scale that fits inside the box, with margin. */
static int fit_px(const touch_zone_t *z)
{
	int len = (int)strlen(z->label);
	if (len == 0) return 1;
	int px_w = (z->w - 6) / (8 * len);
	int px_h = (z->h - 6) / 8;
	int px = px_w < px_h ? px_w : px_h;
	if (px < 1) px = 1;
	if (px > 3) px = 3;
	return px;
}

static void draw_zone(const touch_zone_t *z, uint8_t *canvas, int cw, int ch, bool inverted)
{
	uint8_t fg = inverted ? 0xFF : 0x00;
	uint8_t bg = inverted ? 0x00 : 0xFF;

	ui_fill(canvas, cw, ch, z->x, z->y, z->w, z->h, bg);
	ui_rect(canvas, cw, ch, z->x, z->y, z->w, z->h, 0x00);
	if (z->label && z->label[0]) {
		int px = z->fixed_px > 0 ? z->fixed_px : fit_px(z);
		int tw = ui_text_width(z->label, px);
		int th = 8 * px;
		ui_text(canvas, cw, ch,
			z->x + (z->w - tw) / 2,
			z->y + (z->h - th) / 2,
			z->label, px, fg);
	}
}

void overlay_draw(const overlay_t *o, uint8_t *canvas, int cw, int ch, bool ff_on)
{
	ui_fill(canvas, cw, ch, 0, o->area_y, cw, ch - o->area_y, 0xFF);
	for (int i = 0; i < o->count; i++) {
		const touch_zone_t *z = &o->zones[i];
		draw_zone(z, canvas, cw, ch, ff_on && z->special == OV_FF);
	}
}

void overlay_draw_special(const overlay_t *o, uint8_t *canvas, int cw, int ch,
			  ov_special_t which, bool inverted,
			  int *ox, int *oy, int *ow, int *oh)
{
	*ox = *oy = *ow = *oh = 0;
	for (int i = 0; i < o->count; i++) {
		const touch_zone_t *z = &o->zones[i];
		if (z->special != which) continue;
		draw_zone(z, canvas, cw, ch, inverted);
		*ox = z->x; *oy = z->y; *ow = z->w; *oh = z->h;
		return;
	}
}

uint8_t overlay_map(const overlay_t *o, const plat_input_t *in, ov_specials_t *sp)
{
	uint8_t bits = in->direct_buttons;
	if (sp) memset(sp, 0, sizeof *sp);

	for (int t = 0; t < in->count; t++) {
		int px = in->pts[t].x, py = in->pts[t].y;
		for (int i = 0; i < o->count; i++) {
			const touch_zone_t *z = &o->zones[i];
			if (px < z->x || px >= z->x + z->w ||
			    py < z->y || py >= z->y + z->h)
				continue;
			switch (z->special) {
			case OV_MENU:    if (sp) sp->menu = true; break;
			case OV_DEGHOST: if (sp) sp->deghost = true; break;
			case OV_FF:      if (sp) sp->ff   = true; break;
			default:         bits |= z->bit; break;
			}
		}
	}
	return bits;
}
