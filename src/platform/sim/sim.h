/*
 * sim.h — the knobs for driving the fake "sim" platform in a test.
 *
 * The sim does everything platform.h asks for, but purely in memory: the screen
 * is just a buffer, time is made up (every sleep just fast-forwards the clock,
 * so a test runs instantly and the same way every time), and "input" is a little
 * script keyed to how many times the app has polled. It can snapshot the screen
 * to a PGM image on demand. All of which means you can run the real app end to
 * end and look at the actual pixels without a Kindle or even an SDL window.
 */
#ifndef SIM_H
#define SIM_H

#include <stdint.h>

typedef enum {
	SIM_TOUCH,     /* a,b = x,y : one contact held from now on */
	SIM_RELEASE,   /* lift all contacts */
	SIM_KEY_ENTER,
	SIM_KEY_UP,
	SIM_KEY_DOWN,
	SIM_KEY_MENU,
	SIM_SNAP,      /* write current canvas to PGM file `name` */
	SIM_QUIT       /* request app exit */
} sim_evt_type_t;

/* Call before app_run(): choose framebuffer size and reset the script. */
void sim_set_fb(int w, int h);

/* Schedule an event to fire when plat_input_poll is called for the `at`-th
 * time (0-based). Multiple events may share the same poll index. */
void sim_at(int at_poll, sim_evt_type_t type, int a, int b, const char *name);

/* Records from the last present, for test assertions. */
int  sim_last_present_mode(void);   /* plat_refresh_t of the last present */
int  sim_present_count(void);

#endif /* SIM_H */
