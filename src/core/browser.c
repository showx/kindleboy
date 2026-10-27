/*
 * browser.c — reading the ROM folder, drawing the list, and handling taps.
 */
#include "browser.h"
#include "ui.h"
#include "status.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

/* Layout is derived once and shared by draw and hit-testing so they never
 * disagree. */
typedef struct {
	int title_h;
	int list_y, list_h;
	int row_h, rows_visible;
	int btn_h;
	int up_y, down_y;   /* scroll button rows (full width) */
	int exit_x, exit_w; /* EXIT button, in the title bar, right-aligned */
	int px;             /* text scale */
} geom_t;

static void compute_geom(int cw, int ch, geom_t *g)
{
	g->px      = (cw >= 800) ? 3 : 2;
	g->title_h = 12 * g->px;
	g->exit_w  = 4 * 8 * g->px + 6 * g->px;   /* "EXIT" plus padding */
	g->exit_x  = cw - g->exit_w;
	g->btn_h   = 10 * g->px;
	g->row_h   = 11 * g->px;
	g->up_y    = g->title_h;
	g->list_y  = g->up_y + g->btn_h;
	g->down_y  = ch - g->btn_h;
	g->list_h  = g->down_y - g->list_y;
	g->rows_visible = g->list_h / g->row_h;
	if (g->rows_visible < 1) g->rows_visible = 1;
	(void)cw;
}

static int ci_cmp(const void *a, const void *b)
{
	const char *x = a, *y = b;
	for (; *x && *y; x++, y++) {
		int cx = *x, cy = *y;
		if (cx >= 'A' && cx <= 'Z') cx += 32;
		if (cy >= 'A' && cy <= 'Z') cy += 32;
		if (cx != cy) return cx - cy;
	}
	return (unsigned char)*x - (unsigned char)*y;
}

static bool has_gb_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return false;
	return (ci_cmp(dot, ".gb") == 0) || (ci_cmp(dot, ".gbc") == 0);
}

int browser_scan(browser_t *b, const char *dir)
{
	memset(b, 0, sizeof *b);
	strncpy(b->dir, dir, sizeof b->dir - 1);

	DIR *d = opendir(dir);
	if (!d) return 0;
	struct dirent *e;
	while ((e = readdir(d)) != NULL && b->count < BROWSER_MAX_ROMS) {
		if (e->d_name[0] == '.') continue;
		if (!has_gb_ext(e->d_name)) continue;
		strncpy(b->names[b->count], e->d_name, BROWSER_NAME_LEN - 1);
		b->count++;
	}
	closedir(d);

	qsort(b->names, b->count, BROWSER_NAME_LEN, ci_cmp);
	b->sel = 0;
	b->top = 0;
	return b->count;
}

void browser_path(const browser_t *b, int idx, char *out, int n)
{
	if (idx < 0 || idx >= b->count) { if (n) out[0] = 0; return; }
	snprintf(out, n, "%s/%s", b->dir, b->names[idx]);
}

void browser_draw(const browser_t *b, uint8_t *canvas, int cw, int ch, int batt_pct)
{
	geom_t g; compute_geom(cw, ch, &g);

	ui_fill(canvas, cw, ch, 0, 0, cw, ch, 0xFF);

	/* Title. */
	char title[64];
	snprintf(title, sizeof title, "SELECT A ROM  (%d)", b->count);
	ui_text(canvas, cw, ch, 8, g.px, title, g.px, 0x00);
	ui_fill(canvas, cw, ch, 0, g.title_h - g.px, cw, g.px, 0x00);

	/* Clock + battery, just left of the EXIT button. */
	char sb[40], tb[16];
	status_time_str(tb, sizeof tb, false);
	if (batt_pct >= 0) snprintf(sb, sizeof sb, "%s  %d%%", tb, batt_pct);
	else               snprintf(sb, sizeof sb, "%s", tb);
	int sw = ui_text_width(sb, g.px);
	ui_text(canvas, cw, ch, g.exit_x - sw - 10, g.px, sb, g.px, 0x00);

	/* EXIT button, top-right. */
	ui_rect(canvas, cw, ch, g.exit_x, 0, g.exit_w, g.title_h - g.px, 0x00);
	ui_text(canvas, cw, ch, g.exit_x + 3 * g.px, g.px, "EXIT", g.px, 0x00);

	/* Scroll buttons. */
	ui_rect(canvas, cw, ch, 0, g.up_y, cw, g.btn_h, 0x00);
	ui_text(canvas, cw, ch, cw / 2 - 3 * g.px, g.up_y + g.px, "^", g.px, 0x00);
	ui_rect(canvas, cw, ch, 0, g.down_y, cw, g.btn_h, 0x00);
	ui_text(canvas, cw, ch, cw / 2 - 3 * g.px, g.down_y + g.px, "v", g.px, 0x00);

	if (b->count == 0) {
		ui_text(canvas, cw, ch, 16, g.list_y + g.row_h,
			"No ROMs found.", g.px, 0x00);
		ui_text(canvas, cw, ch, 16, g.list_y + g.row_h * 3,
			"Put .gb files in:", g.px, 0x00);
		ui_text(canvas, cw, ch, 16, g.list_y + g.row_h * 4,
			b->dir, g.px, 0x00);
		return;
	}

	for (int i = 0; i < g.rows_visible; i++) {
		int idx = b->top + i;
		if (idx >= b->count) break;
		int ry = g.list_y + i * g.row_h;
		if (idx == b->sel) {
			ui_fill(canvas, cw, ch, 0, ry, cw, g.row_h, 0x00);      /* invert */
			ui_text(canvas, cw, ch, 8, ry + g.px, b->names[idx], g.px, 0xFF);
		} else {
			ui_text(canvas, cw, ch, 8, ry + g.px, b->names[idx], g.px, 0x00);
		}
	}
}

/* Keep sel visible by adjusting top. */
static void ensure_visible(browser_t *b, int rows_visible)
{
	if (b->sel < 0) b->sel = 0;
	if (b->sel >= b->count) b->sel = b->count - 1;
	if (b->sel < b->top) b->top = b->sel;
	if (b->sel >= b->top + rows_visible) b->top = b->sel - rows_visible + 1;
	if (b->top < 0) b->top = 0;
}

int browser_input(browser_t *b, const plat_input_t *in, int cw, int ch, bool *changed)
{
	geom_t g; compute_geom(cw, ch, &g);
	bool ch_flag = false;
	int launch = -1;

	/* Keyboard (desktop). */
	if (in->key_up)   { b->sel--; ch_flag = true; }
	if (in->key_down) { b->sel++; ch_flag = true; }
	if (in->key_enter && b->count > 0) launch = b->sel;

	/* Touch: act on the rising edge of the first contact. */
	bool touching = in->count > 0;
	if (touching && !b->touch_prev && b->count >= 0) {
		int x = in->pts[0].x, y = in->pts[0].y;
		if (y < g.title_h && x >= g.exit_x) {
			b->touch_prev = touching;
			if (changed) *changed = false;
			return BROWSER_QUIT;                                /* EXIT tapped */
		} else if (y >= g.up_y && y < g.up_y + g.btn_h) {
			b->sel -= g.rows_visible; ch_flag = true;           /* page up */
		} else if (y >= g.down_y && y < g.down_y + g.btn_h) {
			b->sel += g.rows_visible; ch_flag = true;           /* page down */
		} else if (y >= g.list_y && y < g.list_y + g.rows_visible * g.row_h) {
			int row = (y - g.list_y) / g.row_h;
			int idx = b->top + row;
			if (idx >= 0 && idx < b->count) {
				if (idx == b->sel) launch = idx;   /* second tap launches */
				else { b->sel = idx; ch_flag = true; }
			}
		}
	}
	b->touch_prev = touching;

	if (ch_flag) ensure_visible(b, g.rows_visible);
	if (changed) *changed = ch_flag;
	return launch;
}
