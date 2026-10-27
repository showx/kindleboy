/*
 * ui.c — the rectangle and text drawing behind every menu and list.
 */
#include "ui.h"
#include <string.h>

/* Public-domain 8x8 font: font8x8_basic[128][8], one byte per row, bit i is
 * column i (LSB = leftmost). Included here only (non-static global). */
#include "../../vendor/font8x8_basic.h"

static inline void put_px(uint8_t *canvas, int cw, int ch, int x, int y, uint8_t s)
{
	if ((unsigned)x < (unsigned)cw && (unsigned)y < (unsigned)ch)
		canvas[(size_t)y * cw + x] = s;
}

void ui_fill(uint8_t *canvas, int cw, int ch, int x, int y, int w, int h, uint8_t shade)
{
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x >= cw || y >= ch) return;
	if (x + w > cw) w = cw - x;
	if (y + h > ch) h = ch - y;
	if (w <= 0 || h <= 0) return;   /* clamps can go negative; never memset a huge size */
	for (int j = 0; j < h; j++)
		memset(canvas + (size_t)(y + j) * cw + x, shade, (size_t)w);
}

void ui_rect(uint8_t *canvas, int cw, int ch, int x, int y, int w, int h, uint8_t shade)
{
	if (w <= 0 || h <= 0) return;
	ui_fill(canvas, cw, ch, x,         y,         w, 1, shade);
	ui_fill(canvas, cw, ch, x,         y + h - 1, w, 1, shade);
	ui_fill(canvas, cw, ch, x,         y,         1, h, shade);
	ui_fill(canvas, cw, ch, x + w - 1, y,         1, h, shade);
}

void ui_round_fill(uint8_t *canvas, int cw, int ch, int x, int y, int w, int h, int radius, uint8_t shade)
{
	if (w <= 0 || h <= 0) return;
	if (radius < 0) radius = 0;
	if (radius > w / 2) radius = w / 2;
	if (radius > h / 2) radius = h / 2;
	for (int row = 0; row < h; row++) {
		int inset = 0;
		int dy = row < radius ? radius - row - 1 : row >= h - radius ? row - (h - radius) : -1;
		if (dy >= 0) {
			int dx = radius;
			while (dx > 0 && dx * dx + dy * dy > radius * radius) dx--;
			inset = radius - dx;
		}
		ui_fill(canvas, cw, ch, x + inset, y + row, w - inset * 2, 1, shade);
	}
}

void ui_circle(uint8_t *canvas, int cw, int ch, int cx, int cy, int radius, uint8_t shade)
{
	if (radius < 0) return;
	int x = radius;
	for (int y = 0; y <= radius; y++) {
		while (x > 0 && x * x + y * y > radius * radius) x--;
		ui_fill(canvas, cw, ch, cx - x, cy + y, x * 2 + 1, 1, shade);
		if (y) ui_fill(canvas, cw, ch, cx - x, cy - y, x * 2 + 1, 1, shade);
	}
}

void ui_line(uint8_t *canvas, int cw, int ch, int x0, int y0, int x1, int y1, uint8_t shade)
{
	int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
	int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		put_px(canvas, cw, ch, x0, y0, shade);
		if (x0 == x1 && y0 == y1) break;
		int twice = err * 2;
		if (twice >= dy) { err += dy; x0 += sx; }
		if (twice <= dx) { err += dx; y0 += sy; }
	}
}

int ui_text(uint8_t *canvas, int cw, int ch, int x, int y, const char *s, int px, uint8_t shade)
{
	if (px < 1) px = 1;
	for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
		unsigned char c = *p;
		const char *g = (c < 128) ? font8x8_basic[c] : font8x8_basic[0];
		for (int row = 0; row < 8; row++) {
			uint8_t bits = (uint8_t)g[row];
			for (int col = 0; col < 8; col++) {
				if (bits & (1u << col)) {
					int bx = x + col * px;
					int by = y + row * px;
					for (int dy = 0; dy < px; dy++)
						for (int dx = 0; dx < px; dx++)
							put_px(canvas, cw, ch, bx + dx, by + dy, shade);
				}
			}
		}
		x += 8 * px;
	}
	return x;
}

int ui_text_width(const char *s, int px)
{
	if (px < 1) px = 1;
	return (int)strlen(s) * 8 * px;
}
