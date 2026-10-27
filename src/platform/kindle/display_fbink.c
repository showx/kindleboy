/*
 * display_fbink.c — putting pixels on the Kindle's e-ink screen, via FBInk.
 *
 * The core draws into a grayscale buffer we own. When it asks us to show a
 * rectangle, we copy that rectangle out into a packed scratch buffer, hand it to
 * FBInk to write into the framebuffer (without refreshing yet), and then kick
 * off the actual e-ink refresh in whichever waveform mode was asked for.
 *
 * The one subtlety worth calling out: for fast partial updates (A2/DU/DU4) we
 * do NOT wait for the previous refresh — the EPDC handles colliding updates
 * itself, and the wait ioctl can block 150-500ms, which was most of our input
 * lag. This is the same "push before the panel says ready" trick geekmaster's
 * fast-video player and KOReader use. We only wait around the updates where a
 * half-merged frame would actually look broken: before a quality (GL16) or
 * flashing (GC16) refresh, and right after a flash. All single-threaded —
 * it's a single-core chip, a display thread would just add overhead.
 */
#include "../platform.h"
#include "kindle_shared.h"

#include <stdlib.h>
#include <string.h>

#include "fbink.h"

static int      s_fbfd = -1;
static uint8_t *s_canvas;
static uint8_t *s_scratch;
static int      s_w, s_h;
static kindle_disp_t s_disp;

/* One persistent config per usage; FBInk reads wfm_mode/is_flashing/no_refresh
 * per call, so we never need to reinit to switch waveforms. */
static FBInkConfig s_cfg_blit;   /* no_refresh: used for every raw blit */
static FBInkConfig s_cfg_a2;     /* WFM_A2   — FAST */
static FBInkConfig s_cfg_du;     /* WFM_DU   — B/W fallback */
static FBInkConfig s_cfg_du4;    /* WFM_DU4  — 4-gray still promotion, no flash */
static FBInkConfig s_cfg_gl16;   /* WFM_GL16 — QUALITY */
static FBInkConfig s_cfg_flash;  /* WFM_GC16 + flashing — deghost / transitions */

static uint64_t s_last_refresh_us;
static int      s_last_mode;
static bool     s_have_pending;

const kindle_disp_t *kindle_disp(void) { return s_disp.valid ? &s_disp : NULL; }

int plat_init(plat_info_t *out)
{
	FBInkConfig init_cfg = { 0 };
	init_cfg.is_quiet = true;

	s_fbfd = fbink_open();
	if (s_fbfd < 0) { plat_log("fbink_open failed"); return -1; }
	if (fbink_init(s_fbfd, &init_cfg) < 0) { plat_log("fbink_init failed"); return -1; }

	FBInkState st = { 0 };
	fbink_get_state(&init_cfg, &st);
	s_w = (int)st.view_width;
	s_h = (int)st.view_height;
	if (s_w <= 0 || s_h <= 0) { plat_log("bad fb geometry %dx%d", s_w, s_h); return -1; }

	s_disp.w  = s_w;
	s_disp.h  = s_h;
	s_disp.ox = st.view_hori_origin;
	s_disp.oy = st.view_vert_origin;
	s_disp.touch_swap_axes = st.touch_swap_axes;
	s_disp.touch_mirror_x  = st.touch_mirror_x;
	s_disp.touch_mirror_y  = st.touch_mirror_y;
	s_disp.valid = true;
	plat_log("fbink: %dx%d bpp=%u dev=%s mtk=%d swap=%d mx=%d my=%d",
		 s_w, s_h, st.bpp, st.device_name, st.is_mtk,
		 st.touch_swap_axes, st.touch_mirror_x, st.touch_mirror_y);

	s_canvas  = malloc((size_t)s_w * s_h);
	s_scratch = malloc((size_t)s_w * s_h);
	if (!s_canvas || !s_scratch) { plat_log("canvas alloc failed"); return -1; }
	memset(s_canvas, 0xFF, (size_t)s_w * s_h);   /* white */

	/* Build the reusable configs (all quiet). */
	memset(&s_cfg_blit,  0, sizeof s_cfg_blit);
	s_cfg_blit.is_quiet = true;
	s_cfg_blit.no_refresh = true;

	memset(&s_cfg_a2,   0, sizeof s_cfg_a2);   s_cfg_a2.is_quiet = true;   s_cfg_a2.wfm_mode   = WFM_A2;
	memset(&s_cfg_du,   0, sizeof s_cfg_du);   s_cfg_du.is_quiet = true;   s_cfg_du.wfm_mode   = WFM_DU;
	memset(&s_cfg_du4,  0, sizeof s_cfg_du4);  s_cfg_du4.is_quiet = true;  s_cfg_du4.wfm_mode  = WFM_DU4;
	memset(&s_cfg_gl16, 0, sizeof s_cfg_gl16); s_cfg_gl16.is_quiet = true; s_cfg_gl16.wfm_mode = WFM_GL16;
	memset(&s_cfg_flash,0, sizeof s_cfg_flash);s_cfg_flash.is_quiet = true;s_cfg_flash.wfm_mode= WFM_GC16;
	s_cfg_flash.is_flashing = true;

	kindle_input_open();   /* FBInk state is ready, so the touch transform can use it */

	out->fb_w = s_w;
	out->fb_h = s_h;
	out->canvas = s_canvas;
	return 0;
}

void plat_shutdown(void)
{
	kindle_input_close();
	if (s_fbfd >= 0) {
		/* Leave a clean, fully-refreshed screen behind. */
		FBInkConfig c = { 0 }; c.is_quiet = true; c.wfm_mode = WFM_GC16; c.is_flashing = true;
		fbink_cls(s_fbfd, &c, NULL, false);
		fbink_close(s_fbfd);
		s_fbfd = -1;
	}
	free(s_canvas);  s_canvas = NULL;
	free(s_scratch); s_scratch = NULL;
}

static FBInkConfig *cfg_for(plat_refresh_t mode)
{
	switch (mode) {
	case REFRESH_FAST:    return &s_cfg_a2;
	case REFRESH_BW:      return &s_cfg_du;
	case REFRESH_GRAY4:   return &s_cfg_du4;
	case REFRESH_QUALITY: return &s_cfg_gl16;
	case REFRESH_FLASH:   return &s_cfg_flash;
	default:              return &s_cfg_gl16;
	}
}

/* Expected completion time per waveform, for the non-blocking busy estimate. */
static uint64_t wave_us(int mode)
{
	switch (mode) {
	case REFRESH_FAST:    return 115000;   /* A2 is ~120ms */
	case REFRESH_BW:      return 230000;
	case REFRESH_GRAY4:   return 250000;
	case REFRESH_QUALITY: return 450000;
	case REFRESH_FLASH:   return 600000;
	default:              return 450000;
	}
}

void plat_present(int x, int y, int w, int h, plat_refresh_t mode)
{
	if (w <= 0 || h <= 0) return;
	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > s_w) w = s_w - x;
	if (y + h > s_h) h = s_h - y;
	if (w <= 0 || h <= 0) return;

	/* Only wait out the previous refresh when it actually matters: going
	 * into a quality/flashing update (a half-merged flash looks broken), or
	 * coming out of a flash (don't scribble mid-blink). Fast partials just
	 * pile in — the EPDC resolves collisions, and skipping the wait ioctl
	 * (which can block 150-500ms) is where most of the input lag went. */
	bool must_wait = (mode == REFRESH_QUALITY || mode == REFRESH_FLASH ||
			  s_last_mode == (int)REFRESH_FLASH);
	if (s_have_pending && must_wait)
		fbink_wait_for_complete(s_fbfd, LAST_MARKER);

	/* Pack the sub-rect into a contiguous buffer (canvas rows are s_w-strided). */
	for (int j = 0; j < h; j++)
		memcpy(s_scratch + (size_t)j * w,
		       s_canvas + (size_t)(y + j) * s_w + x,
		       (size_t)w);

	short xo = (short)(x + s_disp.ox);
	short yo = (short)(y + s_disp.oy);
	fbink_print_raw_data(s_fbfd, s_scratch, w, h, (size_t)w * h, xo, yo, &s_cfg_blit);
	fbink_refresh(s_fbfd, (uint32_t)yo, (uint32_t)xo, (uint32_t)w, (uint32_t)h, cfg_for(mode));

	s_last_refresh_us = plat_now_us();
	s_last_mode = (int)mode;
	s_have_pending = true;
}

bool plat_refresh_busy(void)
{
	if (!s_have_pending) return false;
	return (plat_now_us() - s_last_refresh_us) < wave_us(s_last_mode);
}

void plat_wait_refresh(void)
{
	if (s_have_pending) {
		fbink_wait_for_complete(s_fbfd, LAST_MARKER);
		s_have_pending = false;
	}
}
