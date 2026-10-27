/*
 * ui.h — just enough drawing to build menus: rectangles and text.
 *
 * Everything works in canvas pixels and clips itself to the edges, so callers
 * can be sloppy about bounds. "shade" is a gray level (0x00 black .. 0xFF white).
 * Text is drawn with a little public-domain 8x8 font, scaled up by px.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>

/* Filled rectangle. */
void ui_fill(uint8_t *canvas, int cw, int ch, int x, int y, int w, int h, uint8_t shade);

/* One-pixel rectangle outline. */
void ui_rect(uint8_t *canvas, int cw, int ch, int x, int y, int w, int h, uint8_t shade);

/* Draw a NUL-terminated string; each glyph is 8*px by 8*px. Returns the x
 * coordinate just past the last glyph. Non-printable/out-of-range chars render
 * as blanks. */
int  ui_text(uint8_t *canvas, int cw, int ch, int x, int y, const char *s, int px, uint8_t shade);

/* Pixel width a string will occupy at the given scale (8*px per char). */
int  ui_text_width(const char *s, int px);

#endif /* UI_H */
