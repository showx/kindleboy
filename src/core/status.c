/*
 * status.c — draw the clock/battery strip.
 */
#include "status.h"
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static int bar_px(int cw) { return cw >= 800 ? 3 : 2; }

int status_height(int cw)
{
	return 20 * bar_px(cw);
}

int status_content_width(int cw) { return cw - 150 * bar_px(cw); }

void status_time_str(char *buf, int n, bool clock_24h)
{
	time_t t = time(NULL);
	struct tm lt;
	struct tm *p =
#if defined(_WIN32)
		localtime(&t);
	if (p) lt = *p; else memset(&lt, 0, sizeof lt);
#else
		localtime_r(&t, &lt);
	(void)p;
#endif
	if (clock_24h) {
		snprintf(buf, n, "%d:%02d", lt.tm_hour, lt.tm_min);
	} else {
		int h = lt.tm_hour % 12; if (h == 0) h = 12;
		snprintf(buf, n, "%d:%02d %s", h, lt.tm_min, lt.tm_hour < 12 ? "AM" : "PM");
	}
}

void status_draw(uint8_t *canvas, int cw, int ch, int batt_pct,
		 bool ff_on, bool low, bool clock_24h)
{
	int px = bar_px(cw);
	int h  = status_height(cw);
	int ty = (h - 8 * px) / 2;

	int right = status_content_width(cw);
	ui_fill(canvas, cw, ch, 8*px, 2*px, right - 8*px, h - 4*px, 0xFF);

	/* Clock, left. */
	char tbuf[16];
	status_time_str(tbuf, sizeof tbuf, clock_24h);
	ui_text(canvas, cw, ch, 8*px, ty, tbuf, px, 0x00);
	(void)ff_on; /* The 3X utility key is the persistent speed indicator. */

	/* Battery, right. */
	if (batt_pct >= 0) {
		char bbuf[16];
		snprintf(bbuf, sizeof bbuf, "%d%%", batt_pct < 0 ? 0 : batt_pct);
		int bp = px > 2 ? 2 : 1;
		int bx = right - 21*px, by = (h - 8*px)/2;
		ui_rect(canvas, cw, ch, bx, by, 18*px, 8*px, 0x00);
		ui_fill(canvas, cw, ch, bx + 18*px, by + 2*px, 2*px, 4*px, 0x00);
		int level = batt_pct > 100 ? 100 : batt_pct;
		ui_fill(canvas, cw, ch, bx + 2*px, by + 2*px, 14*px*level/100, 4*px, 0x00);
		int tw = ui_text_width(bbuf, bp), tx = bx - tw - 4*px;
		if (low) ui_fill(canvas, cw, ch, tx - 2, (h - 8*bp)/2 - 2, tw + 4, 8*bp + 4, 0x00);
		ui_text(canvas, cw, ch, tx, (h - 8*bp)/2, bbuf, bp, low ? 0xFF : 0x00);
	}
}
