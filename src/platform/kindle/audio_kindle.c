/*
 * audio_kindle.c — optional sound on the Kindle, the simple and robust way.
 *
 * The Kindle has no speaker or headphone jack; audio only comes out over
 * Bluetooth, and that path is owned by the framework. Rather than link a fragile
 * ALSA library into our static binary, we pipe raw PCM to a player process
 * (aplay by default) — exactly how the lightweight Kindle audio hacks do it. If
 * the player isn't there, or dies, writes just fail and we go quiet; nothing
 * here can ever crash the emulator, which is the whole point of keeping sound
 * optional and best-effort.
 */
#include "../platform.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

static FILE *s_pipe;
static int   s_fd = -1;

bool plat_audio_open(const char *cmd, int rate, int channels)
{
	(void)rate; (void)channels;   /* the format is baked into the player command */
	if (s_pipe) return true;
	if (!cmd || !cmd[0]) return false;

	/* If the player dies, writing to the pipe raises SIGPIPE, which would kill
	 * us by default. Ignore it so a dead player just yields EPIPE we can drop. */
	signal(SIGPIPE, SIG_IGN);

	s_pipe = popen(cmd, "w");
	if (!s_pipe) { plat_log("audio: popen failed: %s", cmd); return false; }

	/* Non-blocking so a backed-up sink drops samples instead of stalling the
	 * game. We write raw via the fd and never touch stdio buffering. */
	s_fd = fileno(s_pipe);
	int fl = fcntl(s_fd, F_GETFL, 0);
	if (fl >= 0) fcntl(s_fd, F_SETFL, fl | O_NONBLOCK);

	plat_log("audio: pipe open (%d Hz, %d ch): %s", rate, channels, cmd);
	return true;
}

void plat_audio_write(const int16_t *samples, int n_samples)
{
	if (!s_pipe || s_fd < 0 || n_samples <= 0) return;
	/* Best-effort: a short write or EAGAIN just drops those samples. */
	ssize_t r = write(s_fd, samples, (size_t)n_samples * sizeof(int16_t));
	(void)r;
}

void plat_audio_close(void)
{
	if (s_pipe) { pclose(s_pipe); s_pipe = NULL; s_fd = -1; }
}
