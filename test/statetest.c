/*
 * statetest.c — makes sure save states actually work, and that a bad save file
 * can't crash us.
 *
 *  1. Do they capture everything? Save at one point, let the game run on, load
 *     the save back, and run the same stretch again — if the state was complete,
 *     you land on the exact same frame. (It does.)
 *  2. Are bad files safe? A truncated or garbage .st has to be turned away
 *     without leaving the emulator in a broken state you can't run.
 */
#include "../src/core/emu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void plat_log(const char *fmt, ...)
{
	va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}

static uint32_t cksum(const emu_t *e)
{
	uint32_t s = 2166136261u;
	for (int y = 0; y < GB_H; y++)
		for (int x = 0; x < GB_W; x++) { s ^= e->lcd[y][x]; s *= 16777619u; }
	return s;
}

static void run(emu_t *e, int n) { for (int i = 0; i < n; i++) { emu_run_frame(e, 0); emu_frame_consumed(e); } }

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: statetest <rom>\n"); return 2; }
	int pass = 0, fail = 0;
	#define CHECK(cond, msg) do { if (cond) { printf("PASS %s\n", msg); pass++; } else { printf("FAIL %s\n", msg); fail++; } } while (0)

	emu_t e;
	if (emu_load(&e, argv[1]) != EMU_OK) { fprintf(stderr, "load failed\n"); return 1; }
	e.gb.direct.frame_skip = false;

	run(&e, 120);
	uint32_t at_save = cksum(&e);
	CHECK(emu_state_save(&e) == EMU_OK, "state_save returns OK");

	run(&e, 780);                 /* diverge to frame 900 */
	uint32_t at_diverge = cksum(&e);
	CHECK(at_diverge != at_save, "screen actually changed between save and diverge");

	CHECK(emu_state_load(&e) == EMU_OK, "state_load returns OK");
	run(&e, 780);                 /* re-run the same span from the restored state */
	uint32_t rerun = cksum(&e);
	CHECK(rerun == at_diverge, "deterministic re-run reproduces diverge point (state fully captured)");

	/* --- corruption safety --- */
	char st[600];
	snprintf(st, sizeof st, "%s", argv[1]);
	char *dot = strrchr(st, '.'); if (dot) strcpy(dot, ".st"); else strcat(st, ".st");

	/* Truncate the state file to 10 bytes. */
	{ FILE *f = fopen(st, "rb+"); if (f) { /* keep first 10 bytes */
		unsigned char buf[10]; size_t n = fread(buf, 1, 10, f); fclose(f);
		f = fopen(st, "wb"); if (f) { fwrite(buf, 1, n, f); fclose(f); } } }
	int r_trunc = emu_state_load(&e);
	CHECK(r_trunc != EMU_OK, "truncated state file is rejected");
	/* Must still be runnable (pointers intact) — this is the critical fix. */
	emu_run_frame(&e, 0);
	CHECK(1, "emu still runs after rejected load (no crash)");

	/* Wrong magic. */
	{ FILE *f = fopen(st, "wb"); if (f) { for (int i = 0; i < 64; i++) fputc(0xAB, f); fclose(f); } }
	CHECK(emu_state_load(&e) != EMU_OK, "wrong-magic state file is rejected");
	emu_run_frame(&e, 0);
	CHECK(1, "emu still runs after wrong-magic load");

	emu_unload(&e);
	remove(st);
	printf("\n%d passed, %d failed\n", pass, fail);
	return fail ? 1 : 0;
}
