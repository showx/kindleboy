/*
 * render.c — scale the GB frame up and dither it, in one pass over the pixels.
 */
#include "render.h"
#include <string.h>

/* Widest scaled game row we build on the stack (covers every Kindle panel:
 * 160px * scale, and scale tops out around 7 even on the largest). */
#define RENDER_MAX_W (GB_W * 12)

/* 2x2 Bayer threshold matrix. A cell is black iff bayer[dy&1][dx&1] < coverage,
 * where coverage (0..4 black sub-pixels) is derived from the pixel's luma. */
static const uint8_t bayer2[2][2] = { { 0, 2 }, { 3, 1 } };

/* Precomputed 1-bit dither for every 8-bit luma:
 * dither_bw[luma][dy&1][dx&1] == 0x00 (black) or 0xFF (white).
 * The coverage formula rounds (255-luma)/255*4 to the nearest sub-pixel count;
 * for the four DMG grays {0xFF,0xAA,0x55,0x00} it yields exactly {0,1,3,4},
 * i.e. byte-identical output to the old fixed-shade dither, while giving CGB
 * mid-tones a smooth checkerboard. */
static uint8_t dither_bw[256][2][2];
static bool    dither_ready = false;

static void build_dither(void)
{
	for (int l = 0; l < 256; l++) {
		int cov = ((255 - l) * 4 + 127) / 255;   /* 0..4 black sub-pixels */
		for (int dy = 0; dy < 2; dy++)
			for (int dx = 0; dx < 2; dx++)
				dither_bw[l][dy][dx] =
					(bayer2[dy][dx] < cov) ? 0x00 : 0xFF;
	}
	dither_ready = true;
}

void render_layout(render_cfg_t *cfg, int fb_w, int fb_h, int top_reserve, int min_overlay_h)
{
	int s = fb_w / GB_W;
	if (s < 1) s = 1;
	/* Shrink until the game fits between the status bar and the overlay. */
	while (s > 1 && s * GB_H > fb_h - top_reserve - min_overlay_h)
		s--;

	cfg->scale  = s;
	cfg->game_w = s * GB_W;
	cfg->game_h = s * GB_H;
	cfg->dst_x  = (fb_w - cfg->game_w) / 2;         /* center horizontally */
	if (cfg->dst_x < 0) cfg->dst_x = 0;
	cfg->dst_y  = top_reserve + 4;                   /* below the status bar */
	if (cfg->dst_y + cfg->game_h > fb_h)
		cfg->dst_y = 0;
}

void render_game(const emu_t *e, const render_cfg_t *cfg, uint8_t *canvas,
		 int canvas_w, int src_y0, int src_y1, bool quality,
		 int *out_x, int *out_y, int *out_w, int *out_h)
{
	if (!dither_ready) build_dither();

	if (src_y0 < 0) src_y0 = 0;
	if (src_y1 > GB_H - 1) src_y1 = GB_H - 1;
	if (src_y1 < src_y0) { *out_x = cfg->dst_x; *out_y = cfg->dst_y; *out_w = 0; *out_h = 0; return; }

	const int scale = cfg->scale;
	const int ox = cfg->dst_x;
	const int oy = cfg->dst_y;
	const int gw = cfg->game_w;              /* == scale * GB_W */

	/* Each source row expands to `scale` output rows. In QUALITY those rows are
	 * all identical; in FAST the dither only depends on the row's y-parity, so
	 * there are just two distinct rows. Build the distinct row(s) once per
	 * source row and memcpy the rest — same pixels, ~1/scale of the work. A
	 * generous stack buffer covers every Kindle scale; anything larger falls
	 * back to the straightforward path. */
	if (gw <= RENDER_MAX_W) {
		if (quality) {
			uint8_t row[RENDER_MAX_W];
			for (int sy = src_y0; sy <= src_y1; sy++) {
				const uint8_t *srow = e->lcd[sy];
				int dx = 0;
				for (int sx = 0; sx < GB_W; sx++) {
					uint8_t g = srow[sx];   /* luma, straight to GL16 */
					for (int rx = 0; rx < scale; rx++) row[dx++] = g;
				}
				for (int ry = 0; ry < scale; ry++) {
					int dy = oy + sy * scale + ry;
					memcpy(canvas + (size_t)dy * canvas_w + ox, row, (size_t)gw);
				}
			}
		} else {
			uint8_t rowp[2][RENDER_MAX_W];   /* one per y-parity */
			for (int sy = src_y0; sy <= src_y1; sy++) {
				const uint8_t *srow = e->lcd[sy];
				bool built[2] = { false, false };
				for (int ry = 0; ry < scale; ry++) {
					int dy = oy + sy * scale + ry;
					int p = dy & 1;
					if (!built[p]) {
						int dx = 0;
						for (int sx = 0; sx < GB_W; sx++) {
							const uint8_t (*d)[2] = dither_bw[srow[sx]];
							for (int rx = 0; rx < scale; rx++) {
								int dxa = ox + dx;
								rowp[p][dx++] = d[p][dxa & 1];
							}
						}
						built[p] = true;
					}
					memcpy(canvas + (size_t)dy * canvas_w + ox, rowp[p], (size_t)gw);
				}
			}
		}
	} else {
		for (int sy = src_y0; sy <= src_y1; sy++) {
			const uint8_t *srow = e->lcd[sy];
			for (int ry = 0; ry < scale; ry++) {
				int dy = oy + sy * scale + ry;
				uint8_t *drow = canvas + (size_t)dy * canvas_w + ox;
				int dyp = dy & 1, dx = 0;
				for (int sx = 0; sx < GB_W; sx++) {
					if (quality) {
						uint8_t g = srow[sx];
						for (int rx = 0; rx < scale; rx++) drow[dx++] = g;
					} else {
						const uint8_t (*d)[2] = dither_bw[srow[sx]];
						for (int rx = 0; rx < scale; rx++) {
							int dxa = ox + dx;
							drow[dx++] = d[dyp][dxa & 1];
						}
					}
				}
			}
		}
	}

	*out_x = ox;
	*out_y = oy + src_y0 * scale;
	*out_w = cfg->game_w;
	*out_h = (src_y1 - src_y0 + 1) * scale;
}
