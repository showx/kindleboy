/*
 * menu.c — drawing the pause menu and reacting to taps.
 */
#include "menu.h"
#include "ui.h"
#include <string.h>
#include <stdio.h>

static const menu_action_t ITEM_ACTIONS[] = {
	MENU_RESUME, MENU_SAVE_STATE, MENU_LOAD_STATE,
	MENU_TOGGLE_QUALITY, MENU_TOGGLE_AUDIO, MENU_DEGHOST, MENU_QUIT_TO_BROWSER, MENU_QUIT
};
#define ITEM_COUNT ((int)(sizeof ITEM_ACTIONS / sizeof ITEM_ACTIONS[0]))

static void item_label(int i, bool quality_mode, bool audio_on, char *out, int n)
{
	switch (ITEM_ACTIONS[i]) {
	case MENU_RESUME:          snprintf(out, n, "RESUME"); break;
	case MENU_SAVE_STATE:      snprintf(out, n, "SAVE STATE"); break;
	case MENU_LOAD_STATE:      snprintf(out, n, "LOAD STATE"); break;
	case MENU_TOGGLE_QUALITY:  snprintf(out, n, "MODE: %s", quality_mode ? "QUALITY" : "FAST"); break;
	case MENU_TOGGLE_AUDIO:    snprintf(out, n, "SOUND: %s", audio_on ? "ON" : "OFF"); break;
	case MENU_DEGHOST:         snprintf(out, n, "DEGHOST NOW"); break;
	case MENU_QUIT_TO_BROWSER: snprintf(out, n, "QUIT TO LIST"); break;
	case MENU_QUIT:            snprintf(out, n, "EXIT EMULATOR"); break;
	default:                   snprintf(out, n, "?"); break;
	}
}

/* Shared geometry. */
static void geom(int cw, int ch, int *px, int *item_h, int *y0, int *panel_x, int *panel_w)
{
	*px      = (cw >= 800) ? 3 : 2;
	*item_h  = 14 * (*px);
	int total = ITEM_COUNT * (*item_h) + (ITEM_COUNT - 1) * (*px * 2);
	*y0      = (ch - total) / 2;
	if (*y0 < 4) *y0 = 4;
	*panel_w = (cw * 7) / 10;
	if (*panel_w > 520) *panel_w = 520;
	*panel_x = (cw - *panel_w) / 2;
}

void menu_reset(menu_t *m)
{
	m->sel = 0;
	m->touch_prev = false;
}

void menu_draw(const menu_t *m, uint8_t *canvas, int cw, int ch, bool quality_mode, bool audio_on)
{
	int px, item_h, y0, panel_x, panel_w;
	geom(cw, ch, &px, &item_h, &y0, &panel_x, &panel_w);

	ui_fill(canvas, cw, ch, 0, 0, cw, ch, 0xFF);
	ui_text(canvas, cw, ch, panel_x, y0 - 12 * px, "PAUSED", px, 0x00);

	int gap = px * 2;
	for (int i = 0; i < ITEM_COUNT; i++) {
		int iy = y0 + i * (item_h + gap);
		char label[32];
		item_label(i, quality_mode, audio_on, label, sizeof label);
		if (i == m->sel) {
			ui_fill(canvas, cw, ch, panel_x, iy, panel_w, item_h, 0x00);
			ui_text(canvas, cw, ch, panel_x + 12, iy + (item_h - 8 * px) / 2, label, px, 0xFF);
		} else {
			ui_rect(canvas, cw, ch, panel_x, iy, panel_w, item_h, 0x00);
			ui_text(canvas, cw, ch, panel_x + 12, iy + (item_h - 8 * px) / 2, label, px, 0x00);
		}
	}
}

menu_action_t menu_input(menu_t *m, const plat_input_t *in, int cw, int ch, bool *changed)
{
	int px, item_h, y0, panel_x, panel_w;
	geom(cw, ch, &px, &item_h, &y0, &panel_x, &panel_w);
	int gap = px * 2;
	bool ch_flag = false;
	menu_action_t act = MENU_NONE;

	if (in->key_up)   { m->sel = (m->sel + ITEM_COUNT - 1) % ITEM_COUNT; ch_flag = true; }
	if (in->key_down) { m->sel = (m->sel + 1) % ITEM_COUNT; ch_flag = true; }
	if (in->key_enter) act = ITEM_ACTIONS[m->sel];
	if (in->key_menu)  act = MENU_RESUME;   /* menu key toggles back out */

	bool touching = in->count > 0;
	if (touching && !m->touch_prev) {
		int x = in->pts[0].x, y = in->pts[0].y;
		for (int i = 0; i < ITEM_COUNT; i++) {
			int iy = y0 + i * (item_h + gap);
			if (x >= panel_x && x < panel_x + panel_w &&
			    y >= iy && y < iy + item_h) {
				/* One tap activates — menu items are all safe/undoable,
				 * and select-then-confirm felt sluggish on e-ink. */
				m->sel = i;
				act = ITEM_ACTIONS[i];
				break;
			}
		}
	}
	m->touch_prev = touching;

	if (changed) *changed = ch_flag;
	return act;
}
