/*
 * main.c — where it all starts: load the settings, set up a clean exit when the
 * system asks us to quit, and hand off to the app. Same file for both builds.
 */
#include "core/app.h"
#include "core/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* Set by SIGTERM/SIGINT; the Kindle input backend surfaces it as a quit
 * request so saves are flushed on the normal exit path. */
volatile sig_atomic_t g_should_quit = 0;

static void on_signal(int sig) { (void)sig; g_should_quit = 1; }

#if defined(PLATFORM_KINDLE)
#  define DEFAULT_CFG "/mnt/us/extensions/kindleboy/kindleboy.cfg"
#else
#  define DEFAULT_CFG "kindleboy.cfg"
#endif

int main(int argc, char **argv)
{
	signal(SIGTERM, on_signal);
	signal(SIGINT,  on_signal);

	const char *cfg_path = getenv("KINDLEBOY_CFG");
	if (!cfg_path || !cfg_path[0]) cfg_path = DEFAULT_CFG;

	config_t cfg;
	config_defaults(&cfg);
	config_load(&cfg, cfg_path);   /* missing file keeps defaults */

	/* Optional ROM path on the command line boots straight into it (handy on
	 * desktop; on Kindle the browser is the normal path). */
	const char *autostart = (argc > 1) ? argv[1] : NULL;

	int rc = app_run(&cfg, cfg_path, autostart);
	return rc;
}
