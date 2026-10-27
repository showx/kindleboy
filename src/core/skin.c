#include "skin.h"
#include "ui.h"

int skin_unit(int cw) { return cw >= 800 ? 3 : 2; }

void skin_draw(uint8_t *canvas, int cw, int ch, const render_cfg_t *s)
{
	int u = skin_unit(cw);
	ui_fill(canvas, cw, ch, 0, 0, cw, ch, 0xFF);
	ui_round_fill(canvas, cw, ch, 3*u, 2*u, cw - 6*u, ch - 4*u, 14*u, 0xAA);
	ui_round_fill(canvas, cw, ch, 3*u + 1, 2*u + 1, cw - 6*u - 2, ch - 4*u - 2, 14*u, 0xFF);
	int fx = s->dst_x - 14*u, fy = s->dst_y - 12*u;
	int fw = s->game_w + 28*u, fh = s->game_h + 22*u;
	ui_round_fill(canvas, cw, ch, fx + u, fy + 2*u, fw, fh, 10*u, 0xAA);
	ui_round_fill(canvas, cw, ch, fx, fy, fw, fh, 10*u, 0x55);
	const char *caption = "DOT MATRIX / 4-SHADE DISPLAY";
	int px = u > 2 ? 2 : 1;
	ui_text(canvas, cw, ch, s->dst_x + (s->game_w - ui_text_width(caption, px))/2,
		fy + 3*u, caption, px, 0xFF);
	ui_fill(canvas, cw, ch, s->dst_x - 2, s->dst_y - 2, s->game_w + 4, s->game_h + 4, 0x00);
	ui_fill(canvas, cw, ch, s->dst_x, s->dst_y, s->game_w, s->game_h, 0xFF);
	ui_circle(canvas, cw, ch, fx + 7*u, s->dst_y + 12*u, 2*u, 0x00);
	ui_circle(canvas, cw, ch, fx + 7*u - 1, s->dst_y + 12*u - 1, 1, 0xFF);
	ui_text(canvas, cw, ch, fx + 2*u, s->dst_y + 18*u, "BAT", 1, 0xFF);
	int brand_y = s->dst_y + s->game_h + 14*u;
	ui_text(canvas, cw, ch, 16*u, brand_y, "KINDLEBOY", u, 0x00);
	const char *tag = "POCKET GAME SYSTEM";
	ui_text(canvas, cw, ch, cw - 16*u - ui_text_width(tag, px), brand_y + 3*u, tag, px, 0x55);
	/* Six angled speaker slots: hardware detail, never animated. */
	for (int i = 0; i < 6; i++) {
		int x = cw - 56*u + i*6*u, y = ch - 15*u;
		for (int t = 0; t < u; t++) ui_line(canvas, cw, ch, x, y + t, x + 16*u, y - 12*u + t, 0x55);
	}
}
