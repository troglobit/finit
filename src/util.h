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

#ifndef FINIT_UTIL_H_
#define FINIT_UTIL_H_

#include "config.h"
#ifdef HAVE_TERMIOS_H
#include <poll.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/reboot.h>
#include <termios.h>
#endif

/* Define for printheader() from conio.h, used in initctl */
#define SCREEN_WIDTH ttcols
#ifdef _LIBITE_LITE
# include <libite/conio.h>
# include <libite/lite.h>
#else
# include <lite/conio.h>
# include <lite/lite.h>
#endif

#ifndef RB_SW_SUSPEND /* Since Linux 2.5.18, but not in GLIBC until 2.19 */
#define RB_SW_SUSPEND	0xd000fce2
#endif

extern int   debug;			/* defined in finit or initctl */
extern int   ttrows;
extern int   ttcols;
extern char *prognm;

char *progname     (char *arg0);

char *str          (char *fmt, ...);
int   fnread       (char *buf, size_t len, char *fmt, ...);
int   fnwrite      (char *value, char *fmt, ...);
int   fngetint     (char *path, int *val);

int   strtobytes   (char *arg);
char *sig2str      (int sig);
char *code2str     (int code);

void  do_sleep     (unsigned int sec);
long  jiffies      (void);
char *uptime       (long secs, char *buf, size_t len);
char *memsz        (uint64_t sz, char *buf, size_t len);

char *sanitize     (char *arg, size_t len);

#ifdef HAVE_TERMIOS_H
int     ttinit     (void);
int     ttraw      (void);
int     ttcooked   (void);
#else
#define ttinit()   ttcols
#define ttraw()    0
#define ttcooked() 0
#endif

static inline void dbg(char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

static inline char *strterm(char *str, size_t len)
{
	str[len - 1] = 0;
	return str;
}

/* paste dir/file into buf */
static inline int paste(char *buf, size_t len, const char *dir, const char *file)
{
	if (!dir)
		dir = "";
	if (!file)
		file = "";

	return snprintf(buf, len, "%s%s%s", dir, fisslashdir(dir) ? "" : "/", file);
}

#endif /* FINIT_UTIL_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
