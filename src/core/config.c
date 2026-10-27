/*
 * config.c — reading and writing that little key=value settings file.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(config_t *c)
{
	memset(c, 0, sizeof *c);
	c->quality_mode  = false;
	c->autosave_sec  = 60;
	c->deghost_sec   = 60;
	c->idle_pause_sec = 300;
	c->resume        = true;
	c->audio         = false;   /* off by default: saves CPU/battery, needs BT */
	strcpy(c->audio_cmd, "aplay -q -f S16_LE -r 32768 -c 2 -");
	c->scale_override = 0;
	strcpy(c->rom_dir, "/mnt/us/roms/gb");
	c->last_rom[0] = '\0';
}

static void trim(char *s)
{
	char *e = s + strlen(s);
	while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t'))
		*--e = '\0';
	/* leading whitespace */
	char *b = s;
	while (*b == ' ' || *b == '\t') b++;
	if (b != s) memmove(s, b, strlen(b) + 1);
}

int config_load(config_t *c, const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) return -1;
	char line[512];
	while (fgets(line, sizeof line, f)) {
		if (line[0] == '#' || line[0] == '\n') continue;
		char *eq = strchr(line, '=');
		if (!eq) continue;
		*eq = '\0';
		char *key = line, *val = eq + 1;
		trim(key); trim(val);

		if      (!strcmp(key, "quality_mode"))   c->quality_mode = atoi(val) != 0;
		else if (!strcmp(key, "autosave_sec"))   c->autosave_sec = atoi(val);
		else if (!strcmp(key, "deghost_sec"))    c->deghost_sec = atoi(val);
		else if (!strcmp(key, "idle_pause_sec")) c->idle_pause_sec = atoi(val);
		else if (!strcmp(key, "resume"))         c->resume = atoi(val) != 0;
		else if (!strcmp(key, "audio"))          c->audio = atoi(val) != 0;
		else if (!strcmp(key, "audio_cmd"))      { strncpy(c->audio_cmd, val, sizeof c->audio_cmd - 1); c->audio_cmd[sizeof c->audio_cmd - 1] = 0; }
		else if (!strcmp(key, "scale_override")) c->scale_override = atoi(val);
		else if (!strcmp(key, "rom_dir"))        { strncpy(c->rom_dir, val, sizeof c->rom_dir - 1); c->rom_dir[sizeof c->rom_dir - 1] = 0; }
		else if (!strcmp(key, "last_rom"))       { strncpy(c->last_rom, val, sizeof c->last_rom - 1); c->last_rom[sizeof c->last_rom - 1] = 0; }
	}
	fclose(f);
	return 0;
}

int config_save(const config_t *c, const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f) return -1;
	fprintf(f, "# kindleboy preferences\n");
	fprintf(f, "quality_mode=%d\n",   c->quality_mode ? 1 : 0);
	fprintf(f, "autosave_sec=%d\n",   c->autosave_sec);
	fprintf(f, "deghost_sec=%d\n",    c->deghost_sec);
	fprintf(f, "idle_pause_sec=%d\n", c->idle_pause_sec);
	fprintf(f, "resume=%d\n",         c->resume ? 1 : 0);
	fprintf(f, "audio=%d\n",          c->audio ? 1 : 0);
	fprintf(f, "audio_cmd=%s\n",      c->audio_cmd);
	fprintf(f, "scale_override=%d\n", c->scale_override);
	fprintf(f, "rom_dir=%s\n",        c->rom_dir);
	fprintf(f, "last_rom=%s\n",       c->last_rom);
	fclose(f);
	return 0;
}
