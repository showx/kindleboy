/*
 * platform_sim.c — a completely fake "screen" and "touchscreen" that live in
 * memory, so tests can run the real app with no device and no window.
 */
#include "../platform.h"
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_EVENTS 512

typedef struct {
	int at_poll;
	sim_evt_type_t type;
	int a, b;
	char name[64];
} sim_evt_t;

static sim_evt_t s_events[MAX_EVENTS];
static int       s_event_count;

static int       s_fbw = 600, s_fbh = 800;
static uint8_t  *s_canvas;
static uint64_t  s_clock_us;
static int       s_poll_count;

/* current input level state */
static int  s_touch_x, s_touch_y, s_touch_active;
static bool s_quit;

static int  s_last_mode;
static int  s_present_count;

void sim_set_fb(int w, int h)
{
	s_fbw = w; s_fbh = h;
	s_event_count = 0;
	s_poll_count = 0;
	s_clock_us = 0;
	s_touch_active = 0;
	s_quit = false;
	s_present_count = 0;
}

void sim_at(int at_poll, sim_evt_type_t type, int a, int b, const char *name)
{
	if (s_event_count >= MAX_EVENTS) return;
	sim_evt_t *e = &s_events[s_event_count++];
	e->at_poll = at_poll; e->type = type; e->a = a; e->b = b;
	e->name[0] = 0;
	if (name) { strncpy(e->name, name, sizeof e->name - 1); e->name[sizeof e->name - 1] = 0; }
}

int sim_last_present_mode(void) { return s_last_mode; }
int sim_present_count(void)     { return s_present_count; }

static void snapshot(const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f) { perror("sim snapshot"); return; }
	fprintf(f, "P5\n%d %d\n255\n", s_fbw, s_fbh);
	fwrite(s_canvas, 1, (size_t)s_fbw * s_fbh, f);
	fclose(f);
	fprintf(stderr, "sim: snapshot %s\n", path);
}

/* ---- platform.h implementation ------------------------------------------- */

int plat_init(plat_info_t *out)
{
	s_canvas = calloc(1, (size_t)s_fbw * s_fbh);
	if (!s_canvas) return -1;
	memset(s_canvas, 0xFF, (size_t)s_fbw * s_fbh);   /* white */
	out->fb_w = s_fbw; out->fb_h = s_fbh; out->canvas = s_canvas;
	return 0;
}

void plat_shutdown(void)
{
	free(s_canvas); s_canvas = NULL;
}

void plat_present(int x, int y, int w, int h, plat_refresh_t mode)
{
	(void)x; (void)y; (void)w; (void)h;
	s_last_mode = (int)mode;
	s_present_count++;
}

bool plat_refresh_busy(void) { return false; }
void plat_wait_refresh(void) { }

void plat_input_poll(plat_input_t *out)
{
	memset(out, 0, sizeof *out);

	/* Apply all events scheduled for this poll index. */
	for (int i = 0; i < s_event_count; i++) {
		if (s_events[i].at_poll != s_poll_count) continue;
		switch (s_events[i].type) {
		case SIM_TOUCH:   s_touch_active = 1; s_touch_x = s_events[i].a; s_touch_y = s_events[i].b; break;
		case SIM_RELEASE: s_touch_active = 0; break;
		case SIM_KEY_ENTER: out->key_enter = true; break;
		case SIM_KEY_UP:    out->key_up = true; break;
		case SIM_KEY_DOWN:  out->key_down = true; break;
		case SIM_KEY_MENU:  out->key_menu = true; break;
		case SIM_SNAP:      snapshot(s_events[i].name); break;
		case SIM_QUIT:      s_quit = true; break;
		}
	}
	s_poll_count++;

	if (s_touch_active) {
		out->count = 1;
		out->pts[0].x = s_touch_x;
		out->pts[0].y = s_touch_y;
		out->pts[0].id = 0;
	}
	out->quit_requested = s_quit;
}

uint64_t plat_now_us(void) { return s_clock_us; }

void plat_sleep_us(uint64_t us) { s_clock_us += us; }

void plat_input_wait(int timeout_ms) { s_clock_us += (uint64_t)timeout_ms * 1000; }

int plat_battery_percent(void) { return 77; }   /* fake, so snapshots show a value */

/* No audio in the headless sim. */
bool plat_audio_open(const char *cmd, int rate, int channels) { (void)cmd; (void)rate; (void)channels; return false; }
void plat_audio_write(const int16_t *samples, int n_samples)  { (void)samples; (void)n_samples; }
void plat_audio_close(void) { }

void plat_log(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}
