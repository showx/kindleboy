/*
 * browser.h — the "pick a game" screen.
 *
 * Looks through a folder for .gb / .gbc files and shows them as a scrollable
 * list. Tap a row to launch it; the Up/Down buttons (or the arrow keys on the
 * desktop) scroll and move the highlight. It tracks finger-down itself so one
 * tap counts once, not once per frame.
 */
#ifndef BROWSER_H
#define BROWSER_H

#include <stdbool.h>
#include "../platform/platform.h"

#define BROWSER_MAX_ROMS 512
#define BROWSER_NAME_LEN 96

typedef struct {
	char dir[256];
	char names[BROWSER_MAX_ROMS][BROWSER_NAME_LEN];  /* filenames only */
	int  count;
	int  sel;                 /* highlighted index */
	int  top;                 /* first visible row */
	bool touch_prev;          /* for rising-edge tap detection */
} browser_t;

/* Scan dir for ROMs (sorted). Returns the number found. */
int  browser_scan(browser_t *b, const char *dir);

/* Draw the list into the canvas. batt_pct (<0 to hide) and the clock are shown
 * in the header so the status is visible on the game list too. */
void browser_draw(const browser_t *b, uint8_t *canvas, int cw, int ch, int batt_pct);

#define BROWSER_QUIT (-2)   /* returned when the EXIT button is tapped */

/* Process one poll of input. Returns the index of a ROM to launch, BROWSER_QUIT
 * if EXIT was tapped, or -1 for nothing. Sets *changed when the visible
 * list/selection changed (so the caller knows to redraw — important on e-ink
 * where we only refresh on change). */
int  browser_input(browser_t *b, const plat_input_t *in, int cw, int ch, bool *changed);

/* Build the absolute path of names[idx] into out. */
void browser_path(const browser_t *b, int idx, char *out, int n);

#endif /* BROWSER_H */
