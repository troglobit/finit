/* Misc. utility functions for finit and its plugins
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

#include <ctype.h>		/* isblank() */
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <lite/lite.h>
#include <lite/conio.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "utmp-api.h"

int _slup = 0;			/* INTERNAL: Is syslog up yet? */

char *strip_line(char *line)
{
	char *ptr;

	/* Trim leading whitespace */
	while (*line && isblank(*line))
		line++;

	/* Strip any comment at end of line */
	ptr = line;
	while (*ptr && *ptr != '#')
		ptr++;
	*ptr = 0;

	return line;
}

void runlevel_set(int pre, int now)
{
	utmp_set_runlevel(pre, now);
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
	int i, pos = 1;
	static char lvl[20];

	memset(lvl, 0, sizeof(lvl));
	lvl[0] = '[';

	for (i = 0; i < 10; i++) {
		if (ISSET(levels, i)) {
			if (runlevel == i)
				pos = strlcat(lvl, "\e[1m", sizeof(lvl));

			if (i == 0)
				lvl[pos++] = 'S';
			else
				lvl[pos++] = '0' + i;

			if (runlevel == i)
				pos = strlcat(lvl, "\e[0m", sizeof(lvl));
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
	const char warning[] = " \e[7m[WARN]\e[0m\n";
	const char pending[] = " \e[1m[ \\/ ]\e[0m\n";
	const char dots[] = " .....................................................................";

	if (silent)
		return;

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

	case 2:
		write(STDERR_FILENO, warning, sizeof(warning));
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

/*
 * Log to stderr until syslogd has started, then openlog() and continue
 * logging as a regular daemon.
 *
 * TODO: Log to /dev/kmsg instead of stderr until syslogd has started
 *       Need to convert facility+prio => "<VAL> msg"
 */
void logit(int prio, const char *fmt, ...)
{
    va_list    ap;

    va_start(ap, fmt);
    if (!_slup && !fexist("/dev/log")) {
	    FILE *fp;

	    fp = fopen("/dev/kmsg", "w");
	    if (fp) {
		    fprintf(fp, "<%d>finit[1]:", LOG_DAEMON | prio);
		    vfprintf(fp, fmt, ap);
		    fclose(fp);
	    } else {
		    fprintf(stderr, "Failed opening /dev/kmsg for appending ...\n");
		    if (prio <= LOG_ERR)
			    vfprintf(stderr, fmt, ap);
	    }
	    va_end(ap);
	    return;
    }

    if (!_slup) {
	    openlog("finit", LOG_PID, LOG_DAEMON);
	    _slup = 1;
    }

    vsyslog(prio, fmt, ap);
    va_end(ap);
}

int getuser(char *username, char **home)
{
#ifdef ENABLE_STATIC
	*home = "/";		/* XXX: Fixme */
	return fgetint("/etc/passwd", "x:\n", username);
#else
	struct passwd *usr;

	if (!username || (usr = getpwnam(username)) == NULL)
		return -1;

	*home = usr->pw_dir;
	return  usr->pw_uid;
#endif
}

int getgroup(char *group)
{
#ifdef ENABLE_STATIC
	return fgetint("/etc/group", "x:\n", group);
#else
	struct group *grp;

	if ((grp = getgrnam(group)) == NULL)
		return -1;

	return grp->gr_gid;
#endif
}

/* Signal safe sleep ... we get a lot of SIGCHLD at reboot */
void do_sleep(unsigned int sec)
{
	while ((sec = sleep(sec)))
		;
}

void set_hostname(char **hostname)
{
	FILE *fp;

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

#ifndef HAVE_GETFSENT
static lfile_t *fstab = NULL;

int setfsent(void)
{
	if (fstab)
		lfclose(fstab);

	fstab = lfopen("/etc/fstab", " \t\n");
	if (!fstab)
		return 0;

	return 1;
}

struct fstab *getfsent(void)
{
	static struct fstab fs;

	fs.fs_spec    = lftok(fstab);
	if (fs.fs_spec == NULL)
		return NULL;

	fs.fs_file    = lftok(fstab);
	fs.fs_vfstype = lftok(fstab);
	fs.fs_mntops  = lftok(fstab);
	fs.fs_type    = "rw";
	fs.fs_freq    = atoi(lftok(fstab) ?: "0");
	fs.fs_passno  = atoi(lftok(fstab) ?: "0");

	return &fs;
}

void endfsent(void)
{
	if (fstab)
		lfclose(fstab);

	fstab = NULL;
}
#endif	/* HAVE_GETFSENT */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
