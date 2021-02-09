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
#ifdef HAVE_TERMIOS_H		/* for screen_width() */
#include <poll.h>
#include <stdio.h>
#include <termios.h>
#endif

extern int   screen_rows;
extern int   screen_cols;
extern char *prognm;

#define SCREEN_WIDTH screen_cols
#include <lite/conio.h>

char *progname     (char *arg0);
int   echo         (char *file, int append, char *fmt, ...);
int   strtobytes   (char *arg);
void  do_sleep     (unsigned int sec);

long  jiffies      (void);
char *uptime       (long secs, char *buf, size_t len);

char *sanitize     (char *arg, size_t len);

void  screen_init  (void);
void  screen_exit  (void);
int   screen_width (void);

static inline char *strterm(char *str, size_t len)
{
	str[len - 1] = 0;
	return str;
}

#endif /* FINIT_UTIL_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
