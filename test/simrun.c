/*
 * simrun.c — run the whole app against the fake screen and save what it draws.
 *
 *   simrun play    <rom>      jump straight into the game; snapshot game + menu
 *   simrun browser <rom_dir>  start on the game list; tap the first one; snapshot
 *
 * Snapshots land as PGM images in the current directory. This is how the UI and
 * the dithering got checked without ever building for the Kindle.
 */
#include "../src/core/app.h"
#include "../src/core/config.h"
#include "../src/platform/sim/sim.h"
#include <stdio.h>
#include <string.h>

static void dirname_of(const char *path, char *out, int n)
{
	strncpy(out, path, n - 1); out[n - 1] = 0;
	char *s1 = strrchr(out, '/'), *s2 = strrchr(out, '\\');
	char *s = s1 > s2 ? s1 : s2;
	if (s) *s = 0; else strcpy(out, ".");
}

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: simrun play|browser <path>\n"); return 2; }

	config_t cfg;
	config_defaults(&cfg);
	sim_set_fb(600, 800);   /* Kindle Basic (KT4) geometry */

	if (!strcmp(argv[1], "play")) {
		char dir[256]; dirname_of(argv[2], dir, sizeof dir);
		strncpy(cfg.rom_dir, dir, sizeof cfg.rom_dir - 1);
		sim_at(25,  SIM_SNAP, 0, 0, "sim_play_fast.pgm");  /* before quiescent promo */
		sim_at(90,  SIM_SNAP, 0, 0, "sim_play.pgm");
		sim_at(95,  SIM_KEY_MENU, 0, 0, NULL);
		sim_at(97,  SIM_SNAP, 0, 0, "sim_menu.pgm");
		sim_at(100, SIM_QUIT, 0, 0, NULL);
		app_run(&cfg, "sim.cfg", argv[2]);
	} else {
		strncpy(cfg.rom_dir, argv[2], sizeof cfg.rom_dir - 1);
		sim_at(1,   SIM_SNAP, 0, 0, "sim_browser.pgm");
		sim_at(3,   SIM_TOUCH, 300, 55, NULL);   /* tap first row */
		sim_at(4,   SIM_RELEASE, 0, 0, NULL);
		sim_at(120, SIM_SNAP, 0, 0, "sim_browser_play.pgm");
		sim_at(130, SIM_QUIT, 0, 0, NULL);
		app_run(&cfg, "sim.cfg", NULL);
	}

	printf("presents=%d last_mode=%d\n", sim_present_count(), sim_last_present_mode());
	return 0;
}
