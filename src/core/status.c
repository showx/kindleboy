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
	return 8 * bar_px(cw) + 10;
}

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

	ui_fill(canvas, cw, ch, 0, 0, cw, h, 0xFF);
	ui_fill(canvas, cw, ch, 0, h - px, cw, px, 0x00);   /* bottom divider */

	/* Clock, left. */
	char tbuf[16];
	status_time_str(tbuf, sizeof tbuf, clock_24h);
	int x = ui_text(canvas, cw, ch, 6, ty, tbuf, px, 0x00);

	/* Fast-forward marker, just after the clock. */
	if (ff_on)
		ui_text(canvas, cw, ch, x + 8 * px, ty, ">>", px, 0x00);

	/* Battery, right. */
	if (batt_pct >= 0) {
		char bbuf[16];
		snprintf(bbuf, sizeof bbuf, "%d%%", batt_pct < 0 ? 0 : batt_pct);
		int bw = ui_text_width(bbuf, px);
		int bx = cw - 6 - bw;
		if (low) {
			/* Inverted box as a warning. */
			ui_fill(canvas, cw, ch, bx - 4, ty - 2, bw + 8, 8 * px + 4, 0x00);
			ui_text(canvas, cw, ch, bx, ty, bbuf, px, 0xFF);
		} else {
			ui_text(canvas, cw, ch, bx, ty, bbuf, px, 0x00);
		}
	}
}
