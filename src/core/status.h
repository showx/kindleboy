/*
 * status.h — the little top bar: clock + battery (+ a fast-forward marker).
 *
 * Pure core: the time comes from libc, the battery percentage is passed in
 * (the app reads it through plat_battery_percent and caches it). Drawn straight
 * into the Y8 canvas like everything else.
 */
#ifndef STATUS_H
#define STATUS_H

#include <stdint.h>
#include <stdbool.h>

/* Height in pixels of the status strip at the top of the screen. */
int  status_height(int cw);

/* Draw the strip into [0, status_height). batt_pct < 0 hides the battery.
 * ff_on shows a ">>" marker; low inverts the battery reading as a warning. */
void status_draw(uint8_t *canvas, int cw, int ch, int batt_pct,
		 bool ff_on, bool low, bool clock_24h);

/* Format "5:16 PM" (or "17:16") into buf. */
void status_time_str(char *buf, int n, bool clock_24h);

#endif /* STATUS_H */
