/* Misc. shared utility functions for initctl, reboot and finit
 *
 * Copyright (c) 2016-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"
#include <ctype.h>		/* isprint() */
#include <errno.h>
#include <regex.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#include <sys/sysinfo.h>	/* sysinfo() */
#include <lite/lite.h>		/* strlcat() */
#include "util.h"

int   screen_rows  = 24;
int   screen_cols  = 80;
char *prognm       = NULL;

char *progname(char *arg0)
{
       prognm = strrchr(arg0, '/');
       if (prognm)
	       prognm++;
       else
	       prognm = arg0;

       return prognm;
}

char *str(char *fmt, ...)
{
	static char buf[32];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	return buf;
}

int fnwrite(char *value, char *fmt, ...)
{
	char path[256];
	va_list ap;
	FILE *fp;

	if (!value) {
		errno = EINVAL;
		return -1;
	}

	va_start(ap, fmt);
	vsnprintf(path, sizeof(path), fmt, ap);
	va_end(ap);

	fp = fopen(path, "w");
	if (!fp)
		return -1;

	/* echo(1) always adds a newline */
	if (fputs(value, fp) == EOF ||
	    fputs("\n", fp) == EOF  ||
	    fclose(fp))
		return -1;

	return 0;
}

/*
 * musl libc default to /dev/null/utmp and /dev/null/wtmp, respectively.
 * See https://www.openwall.com/lists/musl/2012/03/04/4 for reasoning.
 * Also, there's no __MUSL__, so we cannot make a libc-specific #ifdef
 */
int has_utmp(void)
{
	return strncmp(_PATH_UTMP, "/dev/null", 9);
}

int strtobytes(char *arg)
{
	int mod = 0, bytes;
	size_t pos;

	if (!arg)
		return -1;

	pos = strspn(arg, "0123456789");
	if (arg[pos] != 0) {
		if (arg[pos] == 'G')
			mod = 3;
		else if (arg[pos] == 'M')
			mod = 2;
		else if (arg[pos] == 'k')
			mod = 1;
		else
			return -1;

		arg[pos] = 0;
	}

	bytes = atoi(arg);
	while (mod--)
		bytes *= 1000;

	return bytes;
}

void do_sleep(unsigned int sec)
{
	struct timespec requested_sleep = {
		.tv_sec = sec,
		.tv_nsec = 0,
	};
	while (nanosleep(&requested_sleep, &requested_sleep))
		;
}

/* Seconds since boot, from sysinfo() */
long jiffies(void)
{
	struct sysinfo si;

	if (!sysinfo(&si))
		return si.uptime;

	return 0;
}

char *uptime(long secs, char *buf, size_t len)
{
	long mins, hours, days, years;
	char y[20] = "", d[20] = "", h[20] = "", m[20] = "", s[20] = "";

	if (!buf) {
		errno = EINVAL;
		return NULL;
	}

	years = secs / 31556926;
	secs  = secs % 31556926;
	days  = secs / 86400;
	secs  = secs % 86400;
	hours = secs / 3600;
	secs  = secs % 3600;
	mins  = secs / 60;
	secs  = secs % 60;

	if (years)
		snprintf(y, sizeof(y), "%ld year", years);
	if (days)
		snprintf(d, sizeof(d), "%ld day", days);
	if (hours)
		snprintf(h, sizeof(h), "%ld hour", hours);
	if (mins)
		snprintf(m, sizeof(m), "%ld min", mins);
	if (secs)
		snprintf(s, sizeof(s), "%ld sec", secs);

	snprintf(buf, len, "%s%s%s%s%s%s%s%s%s",
		 y, years ? " " : "",
		 d, days  ? " " : "",
		 h, hours ? " " : "",
		 m, mins  ? " " : "",
		 s);

	return buf;
}

char *memsz(uint64_t sz, char *buf, size_t len)
{
        int gb, mb, kb, b;

	if (!sz) {
		strlcpy(buf, "--.--", len);
		return buf;
	}

        gb = sz / (1024 * 1024 * 1024);
        sz = sz % (1024 * 1024 * 1024);
        mb = sz / (1024 * 1024);
        sz = sz % (1024 * 1024);
        kb = sz / (1024);
        b  = sz % (1024);

        if (gb)
                snprintf(buf, len, "%d.%dG", gb, mb / 102);
        else if (mb)
                snprintf(buf, len, "%d.%dM", mb, kb / 102);
        else
                snprintf(buf, len, "%d.%dk", kb, b / 102);

        return buf;
}

/*
 * Verify string argument is NUL terminated
 * Verify string is a JOB[:ID], JOB and ID
 * can both be string or number, or combo.
 */
char *sanitize(char *arg, size_t len)
{
	const char *regex = "[a-z0-9_]+[:]?[a-z0-9_]*";
	regex_t preg;
	int rc;

	if (!memchr(arg, 0, len))
		return NULL;

	if (regcomp(&preg, regex, REG_ICASE | REG_EXTENDED))
		return NULL;

	rc = regexec(&preg, arg, 0, NULL, 0);
	regfree(&preg);
	if (rc)
		return NULL;

	return arg;
}

/*
 * Called by initctl, and by finit at boot and shutdown, to
 * (re)initialize the screen size for print() et al.
 */
int screen_init(void)
{
#ifdef HAVE_TERMIOS_H
	struct pollfd fd = { STDIN_FILENO, POLLIN, 0 };
	struct winsize ws = { 0 };
	struct termios tc, saved;

	/*
	 * Basic TTY init, CLOCaL is important or TIOCWINSZ will block
	 * until DCD is asserted, and we won't ever get it.
	 */
	tcgetattr(STDERR_FILENO, &tc);
	saved = tc;
	tc.c_cflag |= (CLOCAL | CREAD);
	tc.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tcsetattr(STDERR_FILENO, TCSANOW, &tc);

	/* 1. Try TIOCWINSZ to query window size from kernel */
	if (!ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)) {
		screen_rows = ws.ws_row;
		screen_cols = ws.ws_col;

		/* Likely doesn't work in PID 1 after kernel starts us ... */
		if (!ws.ws_row && !ws.ws_col)
			goto fallback;
	} else if (!isatty(STDOUT_FILENO)) {
		/* 2. We may be running under watch(1) */
		screen_cols = atonum(getenv("COLUMNS"));
		screen_rows = atonum(getenv("LINES"));
	} else {
	fallback:
		/* 3. ANSI goto + query cursor position trick as fallback */
		fprintf(stderr, "\e7\e[r\e[999;999H\e[6n");

		if (poll(&fd, 1, 300) > 0) {
			int row, col;

			if (scanf("\e[%d;%dR", &row, &col) == 2) {
				screen_cols = col;
				screen_rows = row;
			}
		}

		/* Jump back to where we started (\e7) */
		fprintf(stderr, "\e8");
	}

	tcsetattr(STDERR_FILENO, TCSANOW, &saved);

	/* Sanity check */
	if (screen_cols <= 0)
		screen_cols = 80;
	if (screen_rows <= 0)
		screen_rows = 24;
#endif

	return screen_cols;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
