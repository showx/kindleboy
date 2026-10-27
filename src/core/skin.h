#ifndef SKIN_H
#define SKIN_H
#include "render.h"

/* Static four-shade console shell. Repaint only on screen transitions. */
int skin_unit(int cw);
void skin_draw(uint8_t *canvas, int cw, int ch, const render_cfg_t *screen);
#endif
