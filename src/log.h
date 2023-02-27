/* Finit daemon log functions
 *
 * Copyright (c) 2008-2022  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_LOG_H_
#define FINIT_LOG_H_

#include <syslog.h>

/* Local facility, unused in GNU but available in FreeBSD or sysklogd >= 2.0 */
#ifndef LOG_CONSOLE
#define LOG_CONSOLE  (14<<3)
#endif

#ifndef __FINIT__
#include <err.h>
#include <stdarg.h>
#include <stdio.h>

extern int debug;

static __attribute__ ((format (printf, 1, 2))) inline void dbg(char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#define info(fmt, args...) warnx(fmt, ##args)
#else
/*
 * General log macros, similar to those used by initctl.  Initially intended
 * only for bridging client.c in Finit and initctl.
 */
#define dbg(fmt, args...)      logit(LOG_DEBUG,   "%s():" fmt, __func__, ##args)
#define info(fmt, args...)     logit(LOG_INFO,    "%s():" fmt, __func__, ##args)
#define warnx(fmt, args...)    logit(LOG_WARNING, "%s():" fmt, __func__, ##args)
#define warn(fmt, args...)     logit(LOG_WARNING, "%s():" fmt ": %s", __func__, ##args, strerror(errno))
#define errx(rc, fmt, args...) logit(LOG_ERR,     "%s():" fmt, __func__, ##args)
#define err(rc, fmt, args...)  logit(LOG_ERR,     "%s():" fmt ": %s", __func__, ##args, strerror(errno))

/*
 * DEPRECATED developer error and debug messages, please migrate to above!
 * Will be removed without further warning in a future Finit release.
 */
#define  _d(fmt, args...) logit(LOG_DEBUG,   "%s():" fmt, __func__, ##args)
#define  _w(fmt, args...) logit(LOG_WARNING, "%s():" fmt, __func__, ##args)
#define  _e(fmt, args...) logit(LOG_ERR,     "%s():" fmt, __func__, ##args)
#define _pe(fmt, args...) logit(LOG_ERR,     "%s():" fmt ": %s", __func__, ##args, strerror(errno))

void    log_init (void);
void    log_exit (void);

void    log_debug(void);

void    logit    (int prio, const char *fmt, ...)   __attribute__ ((format (printf, 2, 3)));
void    flog     (char *file, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
#endif

#endif /* FINIT_LOG_H_ */
