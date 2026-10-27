/*
 * system_kindle.c — the boring-but-necessary bits on the Kindle: a clock, a
 * sleep, and a log file.
 */
#include "../platform.h"

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define LOG_PATH   "/mnt/us/kindleboy.log"
#define LOG_CAP    (1024 * 1024)   /* truncate the log past 1 MiB */

uint64_t plat_now_us(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000ull;
}

void plat_sleep_us(uint64_t us)
{
	struct timespec ts;
	ts.tv_sec  = (time_t)(us / 1000000ull);
	ts.tv_nsec = (long)((us % 1000000ull) * 1000ull);
	nanosleep(&ts, NULL);
}

static int read_int_file(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	int v = -1;
	if (fscanf(f, "%d", &v) != 1) v = -1;
	fclose(f);
	return v;
}

int plat_battery_percent(void)
{
	/* powerd stays up across a framework stop, so lipc is the reliable path. */
	FILE *p = popen("lipc-get-prop com.lab126.powerd battLevel 2>/dev/null", "r");
	if (p) {
		int v = -1;
		int got = fscanf(p, "%d", &v);
		pclose(p);
		if (got == 1 && v >= 0 && v <= 100) return v;
	}

	/* Fallback: whatever power_supply exposes a capacity. */
	DIR *d = opendir("/sys/class/power_supply");
	if (d) {
		struct dirent *e;
		char path[256];
		while ((e = readdir(d)) != NULL) {
			if (e->d_name[0] == '.') continue;
			snprintf(path, sizeof path, "/sys/class/power_supply/%s/capacity", e->d_name);
			int v = read_int_file(path);
			if (v >= 0 && v <= 100) { closedir(d); return v; }
		}
		closedir(d);
	}
	return -1;
}

void plat_log(const char *fmt, ...)
{
	static FILE *f;
	static long  written;

	if (!f) {
		f = fopen(LOG_PATH, "a");
		if (!f) return;
		written = ftell(f);
	}
	/* Roll over if the log grew too large. */
	if (written > LOG_CAP) {
		freopen(LOG_PATH, "w", f);
		written = 0;
	}

	va_list ap; va_start(ap, fmt);
	int n = vfprintf(f, fmt, ap);
	va_end(ap);
	fputc('\n', f);
	fflush(f);
	if (n > 0) written += n + 1;
}
