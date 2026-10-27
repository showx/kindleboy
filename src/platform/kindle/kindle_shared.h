/*
 * kindle_shared.h — the little bit the Kindle display and touch code both need.
 *
 * Different Kindles wire up their touch panels differently — axes swapped,
 * mirrored, whatever. Handily, FBInk already figures this out per device. So the
 * display code asks FBInk once at startup and stashes the answer here, and the
 * touch code reads it to correct the coordinates instead of guessing.
 */
#ifndef KINDLE_SHARED_H
#define KINDLE_SHARED_H

#include <stdbool.h>

typedef struct {
	int  w, h;             /* view (drawable) dimensions in px */
	int  ox, oy;           /* view origin offset in the panel */
	bool touch_swap_axes;  /* panel reports X/Y swapped */
	bool touch_mirror_x;   /* (post-swap) X inverted */
	bool touch_mirror_y;   /* (post-swap) Y inverted */
	bool valid;
} kindle_disp_t;

/* Populated by plat_init() in display_fbink.c. */
const kindle_disp_t *kindle_disp(void);

/* Touch backend lifecycle (input_evdev.c), driven from display's
 * plat_init/plat_shutdown so main.c stays platform-agnostic. */
void kindle_input_open(void);
void kindle_input_close(void);

#endif /* KINDLE_SHARED_H */
