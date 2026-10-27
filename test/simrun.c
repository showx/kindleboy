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
#include "../src/core/render.h"
#include "../src/core/overlay.h"
#include "../src/core/status.h"
#include "../src/platform/sim/sim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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
	int w=argc>3?atoi(argv[3]):600, h=argc>4?atoi(argv[4]):800;
	sim_set_fb(w,h);   /* actual KT3 is 600x800 */
	render_cfg_t screen; render_layout(&screen,w,h,status_height(w),h/3);
	sim_set_lcd(screen.dst_x,screen.dst_y,screen.game_w,screen.game_h);
	overlay_t o; overlay_layout(&o,&screen,w,h);
	int mx=0,my=0;
	for (int i=0;i<o.count;i++) if (o.zones[i].special==OV_MENU) {
		mx=o.zones[i].x+o.zones[i].w/2; my=o.zones[i].y+o.zones[i].h/2;
	}

	if (!strcmp(argv[1], "feedback")) {
		char dir[256]; dirname_of(argv[2],dir,sizeof dir);
		strncpy(cfg.rom_dir,dir,sizeof cfg.rom_dir-1);
		sim_set_refresh_timing(true);
		sim_at(1,SIM_SNAP,0,0,"sim_fx_rest.pgm");
		sim_at(2,SIM_BUTTONS,JOYPAD_A,0,NULL);
		sim_at(6,SIM_SNAP,0,0,"sim_fx_a.pgm");
		sim_at(10,SIM_BUTTONS,0,0,NULL);
		sim_at(30,SIM_SNAP,0,0,"sim_fx_released.pgm");
		sim_at(36,SIM_BUTTONS,JOYPAD_UP|JOYPAD_LEFT|JOYPAD_A,0,NULL);
		sim_at(40,SIM_SNAP,0,0,"sim_fx_combo.pgm");
		sim_at(52,SIM_BUTTONS,0,0,NULL);
		sim_at(66,SIM_BUTTONS,JOYPAD_SELECT|JOYPAD_START,0,NULL);
		sim_at(70,SIM_SNAP,0,0,"sim_fx_pills.pgm");
		sim_at(82,SIM_BUTTONS,0,0,NULL);
		sim_at(95,SIM_TOUCH,mx,my,NULL);
		sim_at(96,SIM_RELEASE,0,0,NULL);
		sim_at(97,SIM_SNAP,0,0,"sim_fx_menu.pgm");
		sim_at(100,SIM_QUIT,0,0,NULL);
		app_run(&cfg,"sim.cfg",argv[2]);
		assert(sim_present_mode_count(REFRESH_INPUT)>=6);
		assert(sim_present_mode_count(REFRESH_FAST)>=8);
		printf("PASS feedback refreshes stay outside LCD; %d game presents continue alongside %d control presents\n",
			sim_present_mode_count(REFRESH_FAST),sim_present_mode_count(REFRESH_INPUT));
	} else if (!strcmp(argv[1], "play")) {
		char dir[256]; dirname_of(argv[2], dir, sizeof dir);
		strncpy(cfg.rom_dir, dir, sizeof cfg.rom_dir - 1);
		sim_at(25,  SIM_SNAP, 0, 0, "sim_play_fast.pgm");  /* before quiescent promo */
		sim_at(90,  SIM_SNAP, 0, 0, "sim_play.pgm");
		sim_at(95,  SIM_TOUCH, mx, my, NULL);
		sim_at(96,  SIM_RELEASE, 0, 0, NULL);
		sim_at(97,  SIM_SNAP, 0, 0, "sim_menu.pgm");
		sim_at(100, SIM_QUIT, 0, 0, NULL);
		app_run(&cfg, "sim.cfg", argv[2]);
	} else {
		strncpy(cfg.rom_dir, argv[2], sizeof cfg.rom_dir - 1);
		sim_at(1,   SIM_SNAP, 0, 0, "sim_browser.pgm");
		sim_at(3,   SIM_TOUCH, screen.dst_x+40, screen.dst_y+38*(w>=800?3:2), NULL);
		sim_at(4,   SIM_RELEASE, 0, 0, NULL);
		sim_at(120, SIM_SNAP, 0, 0, "sim_browser_play.pgm");
		sim_at(130, SIM_QUIT, 0, 0, NULL);
		app_run(&cfg, "sim.cfg", NULL);
	}

	printf("presents=%d last_mode=%d\n", sim_present_count(), sim_last_present_mode());
	return 0;
}
