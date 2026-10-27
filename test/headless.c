/*
 * headless.c — the simplest possible way to check the emulator is right: run a
 * ROM for a while and save some frames as images.
 *
 * There's no screen and no input here — it just runs the core and writes the
 * frames you ask for as PGM images (plus a checksum, handy for spotting when
 * output drifts). Point it at dmg-acid2.gb and you can eyeball whether the
 * picture is correct, no Kindle or SDL involved.
 *
 * Usage: headless <rom> <num_frames> [dump_frame ...]
 *   e.g. headless dmg-acid2.gb 120 1 60 120
 * Writes frame_<n>.pgm for each frame you list.
 */
#include "../src/core/emu.h"
#include "../src/core/render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Minimal platform hook used by emu.c. */
void plat_log(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

static void dump_pgm(const emu_t *e, int frame)
{
	char name[64];
	snprintf(name, sizeof name, "frame_%d.pgm", frame);
	FILE *f = fopen(name, "wb");
	if (!f) { perror("fopen pgm"); return; }
	fprintf(f, "P5\n%d %d\n255\n", GB_W, GB_H);
	for (int y = 0; y < GB_H; y++) {
		uint8_t line[GB_W];
		for (int x = 0; x < GB_W; x++)
			line[x] = e->lcd[y][x];
		fwrite(line, 1, GB_W, f);
	}
	fclose(f);
	printf("wrote %s\n", name);
}

static uint32_t frame_checksum(const emu_t *e)
{
	uint32_t s = 2166136261u;   /* FNV-1a */
	for (int y = 0; y < GB_H; y++)
		for (int x = 0; x < GB_W; x++) {
			s ^= e->lcd[y][x];
			s *= 16777619u;
		}
	return s;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <rom> <num_frames> [dump_frame ...]\n", argv[0]);
		return 2;
	}
	const char *rom = argv[1];
	int nframes = atoi(argv[2]);

	emu_t emu;
	int r = emu_load(&emu, rom);
	if (r != EMU_OK) { fprintf(stderr, "emu_load failed: %d\n", r); return 1; }

	/* Rasterise every frame so dumps are complete. */
	emu.gb.direct.frame_skip = false;

	for (int i = 1; i <= nframes; i++) {
		emu_run_frame(&emu, 0);   /* no buttons pressed */
		emu_frame_consumed(&emu);
		for (int a = 3; a < argc; a++) {
			if (atoi(argv[a]) == i) {
				printf("frame %d checksum=%08X\n", i, frame_checksum(&emu));
				dump_pgm(&emu, i);
			}
		}
	}

	printf("final frame checksum=%08X\n", frame_checksum(&emu));
	emu_unload(&emu);
	return 0;
}
