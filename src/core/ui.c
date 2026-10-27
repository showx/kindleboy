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
