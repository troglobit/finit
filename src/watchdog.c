/* Built-in watchdog daemon
 *
 * Copyright (c) 2016-2024  Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#include "watchdog.h"

int running  = 1;
int handover = 0;
int shutdown = 0;

static void sighandler(int signo)
{
	if (signo == SIGTERM)
		handover = 1;
	if (signo == SIGPWR)
		shutdown = 1;

	running = 0;
}

static int init(char *progname, char *devnode)
{
	int fd;

	sprintf(progname, "@finit-watchdog");
	signal(SIGTERM, sighandler);
	signal(SIGPWR,  sighandler);

	openlog(&progname[1], LOG_CONS | LOG_PID, LOG_DAEMON);

	fd = open(devnode, O_WRONLY);
	if (fd == -1)
		return -1;

	return fd;
}

static int loop(int fd, int timeout)
{
	int dummy = 0;
	int period = timeout / 2;

	ioctl(fd, WDIOC_SETTIMEOUT, &timeout);

	while (running) {
		ioctl(fd, WDIOC_KEEPALIVE, &dummy);
		sleep(period);
	}

	/* System is going down, prepare to reboot system on TERM */
	if (shutdown) {
		timeout /= 3;
		ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
	}

	/* External watchdogd wants to take over ... */
	if (handover) {
		syslog(LOG_INFO, "Handing over %s and exiting ...", WDT_DEVNODE);
		ioctl(fd, WDIOC_KEEPALIVE, &dummy);
		if (write(fd, "V", 1) == -1)
			return EX_IOERR;
	} else
		syslog(LOG_ALERT, "System going down ...");

	return EX_OK;
}

int main(int argc, char *argv[])
{
	int fd, rc = EX_OK;

	fd = init(argv[0], WDT_DEVNODE);
	if (fd == -1) {
		syslog(LOG_CRIT, "Failed connecting to %s: %s", WDT_DEVNODE, strerror(errno));
		rc = EX_OSFILE;
		goto done;
	}

	rc = loop(fd, WDT_TIMEOUT);
	while (!handover) {
		/* Waiting for SIGTERM or WDT reset ... */
		sleep(1);

		/* Set lowest possible timeout on SIGPWR */
		ioctl(fd, WDIOC_SETTIMEOUT, &shutdown);
	}
	close(fd);
done:
	closelog();

	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
