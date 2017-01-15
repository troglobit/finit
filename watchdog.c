/* Built-in watchdog daemon
 *
 * Copyright (c) 2016  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <paths.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/watchdog.h>

#include "finit.h"
#include "helpers.h"
#include "util.h"
#include "watchdog.h"

int running  = 1;
int handover = 0;

static int notify(void)
{
	int sd, result = 255;
	ssize_t len;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};
	struct init_request rq;

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sd)
		return -1;

	if (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1)
		goto exit;

	rq.cmd      = INIT_CMD_WDOG_HELLO;
	rq.magic    = INIT_MAGIC;
	rq.runlevel = getpid();
	len         = sizeof(rq);
	if (write(sd, &rq, len) != len)
		goto exit;

	if (read(sd, &rq, len) != len)
		goto exit;

	result = 0;
exit:
	close(sd);
	return result;
}

static void sighandler(int signo)
{
	if (signo == SIGTERM)
		handover = 1;
	running = 0;
}

static int init(char *progname, char *devnode, int timeout)
{
	int fd;

	sprintf(progname, "@finit-watchdog");
	signal(SIGTERM, sighandler);
	signal(SIGPWR,  sighandler);

	fd = open(devnode, O_WRONLY);
	if (fd == -1)
		return -1;

	ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

	return fd;
}

static int loop(int fd)
{
	int done = 0;

	while (running) {
		do_sleep(1);
		ioctl(fd, WDIOC_KEEPALIVE, &done);

		if (!done && !notify())
			done = 1;
	}

	if (handover)
		return !write(fd, "V", 1);

	return 0;
}

void watchdog(char *progname)
{
	if (fork() == 0) {
		int fd, ret;

		fd = init(progname, WDT_DEVNODE, WDT_TIMEOUT);
		if (fd == -1) {
			_pe("Failed connecting to watchdog %s", WDT_DEVNODE);
			_exit(1);
		}

		ret = loop(fd);
		close(fd);

		_exit(ret);
	}
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
