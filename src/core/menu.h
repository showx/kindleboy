/*
 * menu.h — the pause menu.
 *
 * A stack of big, easy-to-tap buttons. Opening it is a natural moment to do a
 * full screen flash (which also cleans up any e-ink ghosting) — the app takes
 * care of that. Like the browser, it does its own finger-down tracking.
 */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include "../platform/platform.h"

typedef enum {
	MENU_NONE = 0,
	MENU_RESUME,
	MENU_SAVE_STATE,
	MENU_LOAD_STATE,
	MENU_TOGGLE_QUALITY,
	MENU_TOGGLE_AUDIO,
	MENU_DEGHOST,
	MENU_QUIT_TO_BROWSER,
	MENU_QUIT
} menu_action_t;

typedef struct {
	int  sel;
	bool touch_prev;
} menu_t;

void          menu_reset(menu_t *m);
void          menu_draw(const menu_t *m, uint8_t *canvas, int cw, int ch, bool quality_mode, bool audio_on);
/* Returns the chosen action (MENU_NONE if nothing this poll). Sets *changed
 * when the highlighted item moved (redraw hint for e-ink). */
menu_action_t menu_input(menu_t *m, const plat_input_t *in, int cw, int ch, bool *changed);

#endif /* MENU_H */
