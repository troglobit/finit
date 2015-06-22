/* Misc. utility functions and C-library extensions for finit and its plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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
#include <grp.h>
#include <pwd.h>
#include <limits.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <utmp.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "libite/lite.h"


/*
 * Helpers to replace system() calls
 */

int makepath(char *path)
{
	char buf[PATH_MAX];
	char *x = buf;
	int ret;

	if (!path) {
		errno = EINVAL;
		return -1;
	}

	do {
		do {
			*x++ = *path++;
		} while (*path && *path != '/');

		*x = 0;
		ret = mkdir(buf, 0777);
	} while (*path && (*path != '/' || *(path + 1))); /* ignore trailing slash */

	return ret;
}

void ifconfig(char *ifname, char *addr, char *mask, int up)
{
	struct ifreq ifr;
	struct sockaddr_in *a = (struct sockaddr_in *)&ifr.ifr_addr;
	int sock;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		return;

	memset(&ifr, 0, sizeof (ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	if (up) {
		if (addr) {
			inet_aton(addr, &a->sin_addr);
			ioctl(sock, SIOCSIFADDR, &ifr);
		}
		if (mask) {
			inet_aton(mask, &a->sin_addr);
			ioctl(sock, SIOCSIFNETMASK, &ifr);
		}
	}

	ioctl(sock, SIOCGIFFLAGS, &ifr);

	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	ioctl(sock, SIOCSIFFLAGS, &ifr);

	close(sock);
}

/**
 * atonum - Convert string to integer
 * @str: String value to convert.
 *
 * Returns:
 * The converted (positive) number, or -1 on error, with @errno set.
 */
int atonum(char *str)
{
	int val;
	char *end;

	if (!str)
		return -1;

	errno = 0;
	val = strtol(str, &end, 10);
	if (errno || end == str)
		return -1;

	return val;
}

/**
 * tempfile - A secure tmpfile() replacement
 *
 * This is the secure replacement for tmpfile() that does not exist in GLIBC.
 * The function uses mkstemp() for security.
 *
 * Returns:
 * An open %FILE pointer, or %NULL on error.
 */
FILE *tempfile(void)
{
	int fd;
	mode_t oldmask;
	char template[] = "/tmp/finit.XXXXXX";

	oldmask = umask(0077);
	fd = mkstemp(template);
	umask(oldmask);
	if (-1 == fd)
		return NULL;

	return fdopen(fd, "rw");
}

static int encode(int lvl)
{
	if (!lvl) return 0;
	return lvl + '0';
}

void runlevel_set(int pre, int now)
{
	struct utmp utent;

	utent.ut_type  = RUN_LVL;
	utent.ut_pid   = (encode(pre) << 8) | (encode(now) & 0xFF);
	strlcpy(utent.ut_user, "runlevel", sizeof(utent.ut_user));

	setutent();
	pututline(&utent);
	endutent();
}

int runlevel_get(void)
{
	int lvl = '?';		/* Non-existing runlevel */
	struct utmp *utent;

	setutent();
	while ((utent = getutent())) {
		if (utent->ut_type == RUN_LVL) {
			lvl = utent->ut_pid & 0xFF;
			break;
		}
	}
	endutent();

	return lvl - '0';
}

/**
 * runlevel_string - Convert a bit encoded runlevel to .conf syntax
 * @levels: Bit encoded runlevels
 *
 * Returns:
 * Pointer to string on the form [2345]
 */
char *runlevel_string(int levels)
{
	int i, pos = 1;;
	static char lvl[10] = "[]";

	for (i = 0; i < 10; i++) {
		if (ISSET(levels, i)) {
			if (i == 0)
				lvl[pos++] = 'S';
			else
				lvl[pos++] = '0' + i;
		}
	}
	lvl[pos++] = ']';
	lvl[pos]   = 0;

	return lvl;
}

static int print_timestamp(void)
{
#if defined(CONFIG_PRINTK_TIME)
	FILE * uptimefile;
	float num1, num2;
	char uptime_str[30];

	if((uptimefile = fopen("/proc/uptime", "r")) == NULL)
		return 1;

	fgets(uptime_str, 20, uptimefile);
	fclose(uptimefile);

	sscanf(uptime_str, "%f %f", &num1, &num2);
	sprintf(uptime_str, "[ %.6f ]", num1);

	write(STDERR_FILENO, uptime_str, strlen(uptime_str));
#endif
	return 0;
}

void print(int action, const char *fmt, ...)
{
	char buf[80];
	size_t len;
	va_list ap;
	const char success[] = " \e[1m[ OK ]\e[0m\n";
	const char failure[] = " \e[7m[FAIL]\e[0m\n";
	const char pending[] = " \e[1m[ \\/ ]\e[0m\n";
	const char dots[] = " .....................................................................";

	if (fmt) {
		va_start(ap, fmt);
		len = vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		delline();
		print_timestamp();

		write(STDERR_FILENO, "\r", 1);
		write(STDERR_FILENO, buf, len);
		write(STDERR_FILENO, dots, 60 - len); /* pad with dots. */
	}

	switch (action) {
	case -1:
		break;

	case 0:
		write(STDERR_FILENO, success, sizeof(success));
		break;

	case 1:
		write(STDERR_FILENO, failure, sizeof(failure));
		break;

	default:
		write(STDERR_FILENO, pending, sizeof(pending));
		break;
	}
}

void print_desc(char *action, char *desc)
{
	print(-1, "%s%s", action ?: "", desc ?: "");
}

int print_result(int fail)
{
	print(!!fail, NULL);
	return fail;
}

#ifndef ENABLE_STATIC
int getuser(char *username)
{
	struct passwd *usr;

	if (!username || (usr = getpwnam(username)) == NULL)
		return -1;

	return usr->pw_uid;
}

int getgroup(char *group)
{
	struct group *grp;

	if ((grp = getgrnam(group)) == NULL)
		return -1;

	return grp->gr_gid;
}
#endif

/* Signal safe sleep ... we get a lot of SIGCHLD at reboot */
void do_sleep(unsigned int sec)
{
	while ((sec = sleep(sec)))
		;
}

void set_hostname(char **hostname)
{
	FILE *fp;

	_d("Set hostname: %s", *hostname);
	fp = fopen("/etc/hostname", "r");
	if (fp) {
		struct stat st;

		if (fstat(fileno(fp), &st)) {
			fclose(fp);
			return;
		}

		*hostname = realloc(*hostname, st.st_size);
		if (!*hostname) {
			fclose(fp);
			return;
		}

		fgets(*hostname, st.st_size, fp);
		chomp(*hostname);
		fclose(fp);
	}

	if (*hostname)
		sethostname(*hostname, strlen(*hostname));
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
