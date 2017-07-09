/* Misc. shared utility functions for initctl, reboot and finit
 *
 * Copyright (c) 2016  Joachim Nilsson <troglobit@gmail.com>
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
#include <string.h>
#include <unistd.h>
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

void do_sleep(unsigned int sec)
{
	while ((sec = sleep(sec)))
		;
}

/* Allowed characters in job/id/name */
static int isallowed(int ch)
{
	return isprint(ch);
}

/* Sanitize user input, make sure to NUL terminate. */
char *sanitize(char *arg, size_t len)
{
	size_t i = 0;

	while (isallowed(arg[i]) && i < len)
		i++;

	if (i + 1 < len) {
		arg[i + 1] = 0;
		return arg;
	}

	if (i > 1 && arg[i] == 0)
		return arg;

	return NULL;
}

void screen_init(void)
{
	initscr(&screen_rows, &screen_cols);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
