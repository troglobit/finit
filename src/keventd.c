/* Listens to kernel events like AC power status and manages sys/ conditions
 *
 * Copyright (c) 2021-2023  Joachim Wiberg <troglobit@gmail.com>
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

#include <errno.h>
#include <dirent.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <linux/types.h>
#include <linux/netlink.h>

#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "cond.h"
#include "pid.h"
#include "util.h"

#define _PATH_SYSFS_PWR  "/sys/class/power_supply"

static int num_ac_online;
static int num_ac;

static int running = 1;
static int level;
static int logon;

int debug;			/* debug in other modules as well */

void logit(int prio, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (logon)
		vsyslog(prio, fmt, ap);
	else if (prio <= level) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}
#define panic(fmt, args...) { logit(LOG_CRIT, fmt ":%s", ##args, strerror(errno)); exit(1); }
#define warn(fmt, args...)  { logit(LOG_WARNING, fmt ":%s", ##args, strerror(errno)); }

static void sys_cond(char *cond, int set)
{
	char oneshot[256];

	snprintf(oneshot, sizeof(oneshot), "%s/%s", _PATH_CONDSYS, cond);
	if (set) {
		if (symlink(_PATH_RECONF, oneshot) && errno != EEXIST)
			warn("failed asserting sys/%s", cond);
	} else {
		if (erase(oneshot) && errno != ENOENT)
			warn("failed asserting sys/%s", cond);
	}
}

static int fgetline(char *path, char *buf, size_t len)
{
	FILE *fp;

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	if (!fgets(buf, len, fp)) {
		fclose(fp);
		return -1;
	}

	chomp(buf);
	fclose(fp);

	return 0;
}

static int check_online(char *online)
{
	int val;

	if (!online)
		return 0;

	val = atoi(online);
	logit(LOG_INFO, "AC %s", val ? "connected" : "disconnected");

	return val;
}

static int is_ac(char *type)
{
	char *types[] = {
		"Mains",
		"USB",
		"BrickID",
		"Wireless",
		NULL
	};
	int i;

	for (i = 0; types[i]; i++) {
		if (!strncmp(type, types[i], strlen(types[i])))
			return 1;
	}

	return 0;
}

static void init(void)
{
	struct dirent **d = NULL;
	char *cond_dirs[] = {
		_PATH_CONDSYS,
		_PATH_CONDSYS "/pwr",
	};
	char path[384];
	int i, n;

	for (i = 0; i < (int)NELEMS(cond_dirs); i++) {
		if (mkpath(cond_dirs[i], 0755) && errno != EEXIST) {
			warn("Failed creating %s condition directory, %s", COND_SYS,
			    cond_dirs[i]);
			return;
		}
	}

	n = scandir(_PATH_SYSFS_PWR, &d, NULL, alphasort);
	for (i = 0; i < n; i++) {
		char *nm = d[i]->d_name;
 		char buf[10];

		snprintf(path, sizeof(path), "%s/%s/type", _PATH_SYSFS_PWR, nm);
		if (!fgetline(path, buf, sizeof(buf)) && is_ac(buf)) {
			num_ac++;

			snprintf(path, sizeof(path), "%s/%s/online", _PATH_SYSFS_PWR, nm);
			if (!fgetline(path, buf, sizeof(buf))) {
				if (check_online(buf))
					num_ac_online++;
			}
		}
		free(d[i]);
	}

	if (n > 0)
		free(d);

	/* if any power_supply is online, or none can be found */
	if (num_ac == 0 || num_ac_online > 0)
		sys_cond("pwr/ac", 1);
}

static void set_logging(int prio)
{
	setlogmask(LOG_UPTO(prio));
	level = prio;
}

static void toggle_debug(int signo)
{
	(void)signo;

	debug ^= 1;
	set_logging(debug ? LOG_DEBUG : LOG_NOTICE);
}

static void shut_down(int signo)
{
	(void)signo;
	running = 0;
}

/*
 * Started by Finit as soon as possible when base filesystem is up,
 * modules have been probed, or insmodded from /etc/finit.conf, so by
 * now we should have /sys/class/power_supply/ available for probing.
 * If none is found we assert /sys/pwr/ac condition anyway, this is what
 * systemd does (ConditionACPower) and also makes most sense.
 */
int main(int argc, char *argv[])
{
	struct sockaddr_nl nls = { 0 };
	struct pollfd pfd;
	char buf[1024];

	if (argc > 1) {
		if (!strcmp(argv[1], "-d"))
			debug = 1;
	}

	if (!debug) {
		openlog("keventd", LOG_PID, LOG_DAEMON);
		set_logging(LOG_NOTICE);
		logon = 1;
	}

	signal(SIGUSR1, toggle_debug);
	signal(SIGTERM, shut_down);
	init();

	pfd.events = POLLIN;
	pfd.fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (pfd.fd == -1)
		panic("failed creating netlink socket");

	nls.nl_family = AF_NETLINK;
	nls.nl_pid    = 0;
	nls.nl_groups = -1;
	if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl)))
		panic("bind failed");

	logit(LOG_DEBUG, "Waiting for events ...");
	while (running) {
		char *path = NULL;
		int ac = 0;
		int i, len;

		if (-1 == poll(&pfd, 1, -1)) {
			if (errno == EINTR)
				continue;

			break;
		}

		len = recv(pfd.fd, buf, sizeof(buf), MSG_DONTWAIT);
		if (len == -1) {
			switch (errno) {
			case EINTR:
				continue;
			case ENOBUFS:
				warn("lost events");
				continue;
			default:
				panic("unhandled");
				continue;
			}
		}
		buf[len] = 0;
		logit(LOG_DEBUG, "%s", buf);

		/* skip libusb events, focus on kernel events/changes */
		if (strncmp(buf, "change@", 7))
			continue;

		/* XXX: currently limited to monitoring this subsystem */
		if (!strstr(buf, "power_supply"))
			continue;

		i = 0;
		while (i < len) {
			char *line = buf + i;

			logit(LOG_DEBUG, "%s", line);
			if (!strncmp(line, "DEVPATH=", 8)) {
				path = &line[8];
				logit(LOG_DEBUG, "Got path %s", path);
			} else if (!strncmp(line, "POWER_SUPPLY_TYPE=", 18)) {
				ac = is_ac(&line[18]);
			} else if (!strncmp(line, "POWER_SUPPLY_ONLINE=", 20) && ac) {
				if (check_online(&line[20])) {
					if (!num_ac_online)
						sys_cond("pwr/ac", 1);
					num_ac_online++;
				} else {
					if (num_ac_online > 0)
						num_ac_online--;
					if (!num_ac_online)
						sys_cond("pwr/ac", 0);
				}
			}

			i += strlen(line) + 1;
		}
	}
	close(pfd.fd);

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
