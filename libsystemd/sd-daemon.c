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
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

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
