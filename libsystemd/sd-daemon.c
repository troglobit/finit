/* SPDX-License-Identifier: MIT-0 */

/*
 * systemd notify protocol, supporting readiness notification on startup
 * and reloading, according to the protocol defined at:
 * https://www.freedesktop.org/software/systemd/man/latest/sd_notify.html
 *
 * This protocol is guaranteed to be stable as per:
 * https://systemd.io/PORTABILITY_AND_STABILITY/
 */

#define _GNU_SOURCE 1
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "sd-daemon.h"

/*
 * NOTE: As per spec., this function does not return POSIX OK(0) but
 *       rather "a positive value", i.e., 1.
 */
int sd_notify(int unset_environment, const char *state)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
	};
	size_t len, sun_len;
	const char *path;
	ssize_t written;
	int sd;

	if (!state)
		return -EINVAL;

	len = strlen(state);
	if (len == 0)
		return -EINVAL;

	/* If the variable is not set, the protocol is a noop */
	path = getenv("NOTIFY_SOCKET");
	if (!path)
		return 0;	/* Not set? Nothing to do */

	/* Only AF_UNIX is supported, with path or abstract sockets */
	if (path[0] != '/' && path[0] != '@')
		return -EAFNOSUPPORT;

	/* Ensure there is room for NUL byte */
	sun_len = strlen(path);
	if (sun_len >= sizeof(sun.sun_path))
		return -E2BIG;

	strlcpy(sun.sun_path, path, sizeof(sun.sun_path));
	if (sun.sun_path[0] == '@')
		sun.sun_path[0] = 0;

	sd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (sd == -1)
		return -errno;

	if (connect(sd, (struct sockaddr *)&sun, offsetof(struct sockaddr_un, sun_path) + sun_len) == -1) {
		close(sd);
		return -errno;
	}

	if (unset_environment)
		unsetenv("NOTIFY_SOCKET");

	written = write(sd, state, len);
	if (written != (ssize_t) len) {
		close(sd);
		return written < 0 ? -errno : -EPROTO;
	}

	return 1;
}

/*
 * Printf-style notification function
 */
int sd_notifyf(int unset_environment, const char *format, ...)
{
	va_list ap;
	char *state;
	int ret;

	if (!format)
		return -EINVAL;

	va_start(ap, format);
	ret = vasprintf(&state, format, ap);
	va_end(ap);

	if (ret < 0)
		return -ENOMEM;

	ret = sd_notify(unset_environment, state);
	free(state);

	return ret;
}

/*
 * PID-specific notification function
 */
int sd_pid_notify(pid_t pid, int unset_environment, const char *state)
{
	char path[256], *env;
	size_t len;
	int ret;

	if (!state)
		return -EINVAL;

	len = strlen(state);
	if (len == 0)
		return -EINVAL;

	if (pid == 0)
		pid = getpid();

	env = getenv("NOTIFY_SOCKET");
	if (env) {
		env = strdup(env);
		if (!env)
			return -errno;
	}

	snprintf(path, sizeof(path), "@run/finit/notify/%d", pid);
	setenv("NOTIFY_SOCKET", path, 1);

	ret = sd_notify(unset_environment, state);

	if (env) {
		setenv("NOTIFY_SOCKET", env, 1);
		free(env);
	}

	return ret;
}

/*
 * PID-specific printf-style notification function
 */
int sd_pid_notifyf(pid_t pid, int unset_environment, const char *format, ...)
{
	va_list ap;
	char *state;
	int ret;

	if (!format)
		return -EINVAL;

	va_start(ap, format);
	ret = vasprintf(&state, format, ap);
	va_end(ap);

	if (ret < 0)
		return -ENOMEM;

	ret = sd_pid_notify(pid, unset_environment, state);
	free(state);

	return ret;
}

/*
 * Socket activation stub - Finit doesn't support socket activation yet
 * Returns 0 (no file descriptors passed)
 */
int sd_listen_fds(int unset_environment)
{
	if (unset_environment) {
		unsetenv("LISTEN_PID");
		unsetenv("LISTEN_FDS");
		unsetenv("LISTEN_FDNAMES");
	}

	return 0;
}

/*
 * Watchdog stub - Finit doesn't support watchdog yet
 * Returns 0 (no watchdog enabled)
 */
int sd_watchdog_enabled(int unset_environment, uint64_t *usec)
{
	if (unset_environment) {
		unsetenv("WATCHDOG_PID");
		unsetenv("WATCHDOG_USEC");
	}

	if (usec)
		*usec = 0;

	return 0;
}

/*
 * System detection function
 * Returns 1 to indicate systemd compatibility (trick applications)
 */
int sd_booted(void)
{
	return 1;
}
