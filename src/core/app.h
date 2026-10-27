/*
 * app.h — the top of the whole thing: the browse/play/pause states, the main
 * loop, and the logic that decides when to actually redraw the e-ink screen.
 */
#ifndef APP_H
#define APP_H

#include "config.h"

/* Run the application to completion. cfg is loaded/saved by the caller around
 * this call; cfg_path is where preferences persist. If autostart_rom is
 * non-NULL the app boots straight into that ROM (desktop dev convenience),
 * otherwise it starts in the ROM browser. Returns 0 on clean exit. */
int app_run(config_t *cfg, const char *cfg_path, const char *autostart_rom);

#endif /* APP_H */
