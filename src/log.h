/* Finit daemon log functions
 *
 * Copyright (c) 2008-2017  Joachim Nilsson <troglobit@gmail.com>
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

#include <err.h>
#include <string.h>		/* strerror() */
#include <syslog.h>

/*
 * Error and debug messages
 *
 * Use of logit() is preferred, but these are guaranteed to display on
 * /dev/console as well.  Useful for critical errors and early debug.
 */
#define   _l(prio, fmt, args...) do { if (_slup) warnx(fmt, ##args); logit(prio, fmt "\n", ##args); } while (0)
#define   _m(prio, fmt, args...) do { _l(prio, "%s() - " fmt, __func__, ##args); } while (0)
#define   _d(fmt, args...)  do { if (debug) { _m(LOG_DEBUG, fmt, ##args); } } while (0)
#define   _w(fmt, args...)  do { _m(LOG_WARNING, fmt, ##args); } while (0)
#define   _e(fmt, args...)  do { _m(LOG_ERR, fmt, ##args); } while (0)
#define  _pe(fmt, args...)  do { _m(LOG_ERR, fmt ". Error %d: %s", ##args, errno, strerror(errno)); } while (0)

extern int _slup;		/* INTERNAL: Is syslog up yet? */

void    logit           (int prio, const char *fmt, ...);

#endif /* FINIT_LOG_H_ */
