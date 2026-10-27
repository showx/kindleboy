/*
 * platform_sdl.c — the desktop version, so you can build and play on a PC
 * instead of flashing the Kindle every time.
 *
 * It opens an SDL2 window, shows the grayscale canvas in it, maps the keyboard
 * to the Game Boy buttons and the menu keys, and treats a mouse click as a
 * touch. It looks exactly like the Kindle build — the core draws all the pixels
 * either way; SDL just gives us a window and a keyboard.
 *
 * The window is 600x800 by default (matching the Kindle Basic); set
 * KINDLEBOY_FB=WxH to change it, or KINDLEBOY_EINK=1 to throttle it down to
 * ~8 fps so you can feel what the real e-ink refresh is like.
 */
#include "../platform.h"
#include "../../core/emu.h"    /* JOYPAD_* */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>

extern volatile sig_atomic_t g_should_quit;   /* defined in main.c */

static SDL_Window   *s_win;
static SDL_Renderer *s_ren;
static SDL_Texture  *s_tex;
static uint8_t      *s_canvas;
static uint32_t     *s_rgb;
static int           s_w, s_h;
static Uint64        s_perf_freq;
static bool          s_eink_preview;
static uint64_t      s_last_present_us;

int plat_init(plat_info_t *out)
{
	s_w = 600; s_h = 800;
	const char *fb = getenv("KINDLEBOY_FB");
	if (fb) { int w, h; if (sscanf(fb, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { s_w = w; s_h = h; } }
	s_eink_preview = getenv("KINDLEBOY_EINK") != NULL;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return -1; }
	s_win = SDL_CreateWindow("KindleBoy", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				 s_w, s_h, SDL_WINDOW_SHOWN);
	if (!s_win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return -1; }
	s_ren = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!s_ren) s_ren = SDL_CreateRenderer(s_win, -1, 0);
	s_tex = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, s_w, s_h);

	s_canvas = malloc((size_t)s_w * s_h);
	s_rgb    = malloc((size_t)s_w * s_h * 4);
	if (!s_canvas || !s_rgb || !s_tex) return -1;
	memset(s_canvas, 0xFF, (size_t)s_w * s_h);

	s_perf_freq = SDL_GetPerformanceFrequency();

	out->fb_w = s_w; out->fb_h = s_h; out->canvas = s_canvas;
	return 0;
}

void plat_shutdown(void)
{
	free(s_rgb); s_rgb = NULL;
	free(s_canvas); s_canvas = NULL;
	if (s_tex) SDL_DestroyTexture(s_tex);
	if (s_ren) SDL_DestroyRenderer(s_ren);
	if (s_win) SDL_DestroyWindow(s_win);
	SDL_Quit();
}

void plat_present(int x, int y, int w, int h, plat_refresh_t mode)
{
	(void)x; (void)y; (void)w; (void)h; (void)mode;

	if (s_eink_preview) {
		uint64_t now = plat_now_us();
		uint64_t gap = (mode == REFRESH_FAST) ? 125000 : 300000;
		if (now - s_last_present_us < gap) SDL_Delay((Uint32)((gap - (now - s_last_present_us)) / 1000));
		s_last_present_us = plat_now_us();
	}

	/* Convert the whole canvas Y8 -> ARGB (present is infrequent). */
	int n = s_w * s_h;
	for (int i = 0; i < n; i++) {
		uint8_t g = s_canvas[i];
		s_rgb[i] = 0xFF000000u | ((uint32_t)g << 16) | ((uint32_t)g << 8) | g;
	}
	SDL_UpdateTexture(s_tex, NULL, s_rgb, s_w * 4);
	SDL_RenderClear(s_ren);
	SDL_RenderCopy(s_ren, s_tex, NULL, NULL);
	SDL_RenderPresent(s_ren);
}

bool plat_refresh_busy(void) { return false; }
void plat_wait_refresh(void) { }

void plat_input_poll(plat_input_t *out)
{
	memset(out, 0, sizeof *out);

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) out->quit_requested = true;
		else if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
			switch (e.key.keysym.sym) {
			case SDLK_UP:     out->key_up = true; break;
			case SDLK_DOWN:   out->key_down = true; break;
			case SDLK_RETURN: out->key_enter = true; break;
			case SDLK_ESCAPE:
			case SDLK_TAB:    out->key_menu = true; break;
			case SDLK_q:      out->quit_requested = true; break;
			default: break;
			}
		}
	}

	const Uint8 *ks = SDL_GetKeyboardState(NULL);
	uint8_t b = 0;
	if (ks[SDL_SCANCODE_UP])    b |= JOYPAD_UP;
	if (ks[SDL_SCANCODE_DOWN])  b |= JOYPAD_DOWN;
	if (ks[SDL_SCANCODE_LEFT])  b |= JOYPAD_LEFT;
	if (ks[SDL_SCANCODE_RIGHT]) b |= JOYPAD_RIGHT;
	if (ks[SDL_SCANCODE_Z])     b |= JOYPAD_B;
	if (ks[SDL_SCANCODE_X])     b |= JOYPAD_A;
	if (ks[SDL_SCANCODE_RETURN])b |= JOYPAD_START;
	if (ks[SDL_SCANCODE_RSHIFT])b |= JOYPAD_SELECT;
	out->direct_buttons = b;

	int mx, my;
	Uint32 ms = SDL_GetMouseState(&mx, &my);
	if (ms & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		if (mx < 0) mx = 0;
		if (mx >= s_w) mx = s_w - 1;
		if (my < 0) my = 0;
		if (my >= s_h) my = s_h - 1;
		out->count = 1;
		out->pts[0].x = mx; out->pts[0].y = my; out->pts[0].id = 0;
	}

	if (g_should_quit) out->quit_requested = true;
}

uint64_t plat_now_us(void)
{
	return (uint64_t)(SDL_GetPerformanceCounter() * 1000000ull / s_perf_freq);
}

void plat_sleep_us(uint64_t us)
{
	if (us >= 1000) SDL_Delay((Uint32)(us / 1000));
}

void plat_input_wait(int timeout_ms)
{
	SDL_Event e;
	if (SDL_WaitEventTimeout(&e, timeout_ms))
		SDL_PushEvent(&e);   /* put it back for plat_input_poll to consume */
}

int plat_battery_percent(void) { return 77; }   /* fake on desktop, to exercise the UI */

/* ---- audio: real SDL output so the whole path is testable by ear ---------- */
static SDL_AudioDeviceID s_audio;

bool plat_audio_open(const char *cmd, int rate, int channels)
{
	(void)cmd;   /* the pipe command is a Kindle concept; desktop uses SDL directly */
	if (s_audio) return true;
	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof want);
	want.freq     = rate;
	want.format   = AUDIO_S16SYS;
	want.channels = (Uint8)channels;
	want.samples  = 1024;
	want.callback = NULL;   /* we push with SDL_QueueAudio */
	s_audio = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
	if (!s_audio) { fprintf(stderr, "audio: SDL_OpenAudioDevice: %s\n", SDL_GetError()); return false; }
	SDL_PauseAudioDevice(s_audio, 0);
	return true;
}

void plat_audio_write(const int16_t *samples, int n_samples)
{
	if (!s_audio || n_samples <= 0) return;
	/* Cap the queue so running ahead of real time can't build unbounded lag. */
	if (SDL_GetQueuedAudioSize(s_audio) > (Uint32)(EMU_AUDIO_RATE * 2 * sizeof(int16_t)))
		return;   /* ~1s buffered already: drop this frame */
	SDL_QueueAudio(s_audio, samples, (Uint32)((size_t)n_samples * sizeof(int16_t)));
}

void plat_audio_close(void)
{
	if (s_audio) { SDL_CloseAudioDevice(s_audio); s_audio = 0; }
}

void plat_log(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}
