/* Misc. utility functions for finit and its plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2025  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"		/* Generated by configure script */

#include <ctype.h>		/* isblank() */
#include <errno.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sysexits.h>
#include <sys/ioctl.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "finit.h"
#include "helpers.h"
#include "conf.h"
#include "log.h"
#include "private.h"
#include "sig.h"
#include "util.h"
#include "utmp-api.h"

static pstyle_t progress_onoff = PROGRESS_DEFAULT;
static pstyle_t progress_style = PROGRESS_DEFAULT;

#ifndef HOSTNAME_PATH
#define HOSTNAME_PATH "/etc/hostname"
#endif

/*
 * Note: the pending status (⋯) must be last item.  Also, ⋯ is not
 *       off-by-one, it's a multi-byte character.
 */
#define STATUS_CLASS {							\
	CHOOSE(" OK ",  " OK ",  "\e[1;32m"),		/* Green  */	\
	CHOOSE("FAIL",  "FAIL",  "\e[1;31m"),		/* Red    */	\
	CHOOSE("WARN",  "WARN",  "\e[1;33m"),		/* Yellow */	\
	CHOOSE(" \\/ ", " ⋯  ", "\e[1;33m"),		/* Yellow */	\
}

#define CHOOSE(x,y,z) x
static const char *status1[] = STATUS_CLASS;
#undef CHOOSE

#define CHOOSE(x,y,z) y
static const char *status2[] = STATUS_CLASS;
#undef CHOOSE

#define CHOOSE(x,y,z) z
static const char *color[] = STATUS_CLASS;

char *console(void)
{
	return _PATH_CONSOLE;
}

void console_init(void)
{
	/* Enable line wrap, if disabled previously, e.g., qemu */
	dprint(STDOUT_FILENO, "\033[?7h", 5);

	/* Reset attributes, background and foreground color  */
	dprint(STDOUT_FILENO, "\033[49m\033[39m\e[2J", 14);

	log_init();
}

ssize_t cprintf(const char *fmt, ...)
{
	const size_t len = strlen(fmt) * 2;
	char buf[len < 256 ? 256 : len];
	size_t size;
	va_list ap;

	va_start(ap, fmt);
	size = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	dprint(STDERR_FILENO, buf, size);

	return size;
}

void tabstospaces(char *line)
{
	if (!line)
		return;

	for (int i = 0; line[i]; i++) {
		if (line[i] == '\t')
			line[i] = ' ';
	}
}

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

void enable_progress(int onoff)
{
	if (onoff)
		progress_style = progress_onoff;
	else
		progress_style = PROGRESS_SILENT;
}

void show_progress(pstyle_t style)
{
	progress_onoff = style;
}

/*
 * Find PRETTY_NAME in /etc/os-release to use as heading at boot
 * and on runlevel changes.  Fallback: "Finit vX.YY"
 */
char *release_heading(void)
{
	char *name = NULL;
	char buf[256];
	FILE *fp;

	fp = fopen("/etc/os-release", "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp)) {
			char *ptr = &buf[12];

			if (strncmp(buf, "PRETTY_NAME=", 12))
				continue;

			if (*ptr == '"' || *ptr == '\'') {
				char *end = &ptr[strlen(ptr) - 1];
				char q = *ptr;

				while (end > ptr && isspace(*end))
					*end-- = 0;

				if (*end == q) {
					*end = 0;
					ptr++;
				}
			}

			name = strdup(ptr);
			break;
		}
		fclose(fp);
	}

	if (!name)
		name = strdup(PACKAGE_NAME " v" PACKAGE_VERSION);

	return name;
}

/*
 * Return screen length of string, not counting escape chars, and
 * accounting for unicode characters as only one screen byte wide
 */
size_t slen(char *string)
{
	unsigned char *buf = (unsigned char *)string;
	size_t len = 0;

	while (*buf) {
		/* Skip ANSI escape sequences */
		if (*buf == '\e') {
			while (*buf && !isalpha(*buf))
				buf++;
			continue;
		}
		/* Skip 3-byte unicode */
		if (*buf == 0xe2) {
			for (int cnt = 3; *buf && cnt >= 0; cnt--)
				buf++;
			continue;
		}

		len++;
		buf++;
	}

	return len;
}

/*
 * ch may be an ascii or unicode character
 */
static char *pad(char *buf, size_t len, char *ch, size_t width)
{
	size_t i = slen(buf);

	strlcat(buf, " ", len);

	width -= 8;		/* Skip leading '[ STAT ]' */
	while (i < width) {
		strlcat(buf, ch, len);
		i++;
	}

	return buf;
}

void print_banner(const char *heading)
{
	char buf[4 * ttcols];

	if (progress_style == PROGRESS_SILENT)
		return;

	buf[0] = 0;
	strlcat(buf, "\r\e[K", sizeof(buf));

	if (progress_style == PROGRESS_CLASSIC) {
		strlcat(buf, "\e[1m", sizeof(buf));
		strlcat(buf, heading, sizeof(buf));
		pad(buf, sizeof(buf), "=", ttcols + 10);
	} else {
		size_t wmax = 80 <= ttcols ? 80 : ttcols;

		/* • • • Foo System ═══════════════════════════════════════════════ */
		/* ⬤ ⬤ ⬤ Foo System ═══════════════════════════════════════════════ */
		/* ⚉ ⚉ ⚉ Foo System ═══════════════════════════════════════════════ */
		/* o o o Foo System ═══════════════════════════════════════════════ */
		/* ◉ ◉ ◉ Foo System ═══════════════════════════════════════════════ */
		/* ⦿⦿⦿ Foo System ═══════════════════════════════════════════════ */
		strlcat(buf, "\e[1;31m● \e[1;33m● \e[1;32m● \e[0m\e[1m ", sizeof(buf));
		strlcat(buf, heading, sizeof(buf));

		/*
		 * Padding with full-width '═' sign from unicode,
		 * we could also use '―' or something else.
		 */
		pad(buf, sizeof(buf), "═", wmax);
	}
	strlcat(buf, "\e[0m\n", sizeof(buf));
	cprintf("%s", buf);
}

static size_t print_timestamp(char *buf, size_t len)
{
#if defined(CONFIG_PRINTK_TIME)
	FILE *fp;
	float stamp, dummy;

	fp = fopen("/proc/uptime", "r");
	if (!fp)
		return;

	fgets(buf, len, fp);
	fclose(fp);

	sscanf(buf, "%f %f", &stamp, &dummy);
	return snprintf(buf, len, "[ %.6f ]", stamp);
#else
	(void)buf;
	(void)len;
	return 0;
#endif
}

static char *status(int rc)
{
	static char buf[64];

	if (rc < 0 || rc >= (int)NELEMS(status1))
		rc = NELEMS(status1) - 1;	/* Default to "⋯" (pending) */

	if (progress_style == PROGRESS_CLASSIC) {
		int hl = 1;

		if (rc == 1 || rc == 2)
			hl = 7;

		snprintf(buf, sizeof(buf), "\e[%dm[%s]\e[0m", hl, status1[rc]);
	} else
		snprintf(buf, sizeof(buf), "\e[1m[%s%s\e[0m\e[1m]\e[0m ", color[rc], status2[rc]);

	return buf;
}

void printv(const char *fmt, va_list ap)
{
	char buf[ttcols];
	size_t len;

	if (!fmt || progress_style == PROGRESS_SILENT)
		return;

	delline();

	buf[0] = 0;
	len = print_timestamp(buf, sizeof(buf));
	vsnprintf(&buf[len], sizeof(buf) - len, fmt, ap);

	if (progress_style == PROGRESS_CLASSIC)
		cprintf("\r%s ", pad(buf, sizeof(buf), ".", sizeof(buf)));
	else
		cprintf("\r\e[K%s%s", status(3), buf);
}

void print(int rc, const char *fmt, ...)
{
	if (progress_style == PROGRESS_SILENT)
		return;

	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		printv(fmt, ap);
		va_end(ap);
	}

	if (rc < 0)
		return;

	if (progress_style == PROGRESS_CLASSIC)
		cprintf("%s\n", status(rc));
	else
		cprintf("\r%s\n", status(rc));
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

void set_hostname(char **hostname)
{
	FILE *fp;

	if (rescue)
		goto done;

	fp = fopen(HOSTNAME_PATH, "r");
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

		if (fgets(*hostname, st.st_size, fp))
			chomp(*hostname);
		fclose(fp);
	}

done:
	if (*hostname) {
		if (sethostname(*hostname, strlen(*hostname)))
			err(1, "Failed sethostname(%s)", *hostname);
	}
}

/*
 * Bring up networking, but only if not single-user or rescue mode
 */
void networking(int updown)
{
	/* No need to report errors if network is already down */
	if (!prevlevel && !updown)
		return;

	if (updown)
		dbg("Setting up networking ...");
	else
		dbg("Taking down networking ...");

	/* Run user network start script if enabled */
	if (updown && network) {
		run_interactive(network, "Starting networking: %s", network);
		goto done;
	}

	/* Debian/Ubuntu/Busybox/RH/Suse */
	if (!whichp("ifup"))
		goto fallback;

	if (fexist("/etc/network/interfaces")) {
		pid_t pid;

		pid = fork();
		if (pid == 0) {
			const char *cmd;
			char buf[256];
			FILE *pp;
			int rc;

			setsid();
			sig_unblock();

			if (updown)
				cmd = "ifup -a 2>&1";
			else if (whichp("ifquery"))
				/* Regular ifodwn supports --force but not -f */
				cmd = "ifdown -a --force 2>&1";
			else
				/* Busybox ifdown support -f, but not --force */
				cmd = "ifdown -a -f 2>&1";

			pp = popen(cmd, "r");
			if (!pp)
				_exit(EX_OSERR);

			while (fgets(buf, sizeof(buf), pp))
				logit(LOG_NOTICE, "network: %s", chomp(buf));

			rc = pclose(pp);
			if (rc == -1)
				_exit(EX_OSERR);
			_exit(WEXITSTATUS(rc));
		}
		cgroup_service("network", pid, NULL);
		print(pid > 0 ? 0 : 1, "%s network interfaces ...",
		      updown ? "Bringing up" : "Taking down");

		goto done;
	}

fallback:
	/* Fall back to bring up at least loopback */
	ifconfig("lo", "127.0.0.1", "255.0.0.0", updown);
done:

	/* Hooks that rely on loopback, or basic networking being up. */
	if (updown) {
		dbg("Calling all network up hooks ...");
		plugin_run_hooks(HOOK_NETWORK_UP);
	} else {
		dbg("Calling all network down hooks ...");
		plugin_run_hooks(HOOK_NETWORK_DN);
	}
}

/*
 * Very basic, and incomplete, check if we're running in a container.
 */
int in_container(void)
{
	const char *containers[] = {
		"lxc",
		"docker",
		"kubepod",
		"unshare",
		"systemd-nspawn"
	};
	const char *files[] = {
		"/run/.containerenv",
		"/.dockerenv"
	};
	static int incont = -1;
	size_t i;
	char *c;

	if (incont > 0)
		return incont;

	c = getenv("container");
	if (c) {
		for (i = 0; i < NELEMS(containers); i++) {
			if (!strcmp(containers[i], c))
				return incont = 1;
		}
	}

	for (i = 0; i < NELEMS(files); i++) {
		if (!access(files[i], F_OK))
			return incont = 1;
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
