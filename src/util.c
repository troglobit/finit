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

#ifdef HAVE_TERMIOS_H		/* for screen_width() */
#include <poll.h>
#include <stdio.h>
#include <termios.h>
#endif

int   screen_setup = -1;
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

#define ESC "\033"
void screen_init(void)
{
#ifdef HAVE_TERMIOS_H
	char buf[42];
	struct termios tc, saved;
	struct pollfd fd = { STDIN_FILENO, POLLIN, 0 };

	if (screen_setup != -1)
		return;

	memset(buf, 0, sizeof(buf));
	tcgetattr(STDERR_FILENO, &tc);
	saved = tc;
	tc.c_cflag |= (CLOCAL | CREAD);
	tc.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	tcsetattr(STDERR_FILENO, TCSANOW, &tc);
	fprintf(stderr, ESC "7" ESC "[r" ESC "[999;999H" ESC "[6n");

	if (poll(&fd, 1, 300) > 0) {
		int row, col;

		if (scanf(ESC "[%d;%dR", &row, &col) == 2) {
			screen_rows  = row;
			screen_cols  = col;
			screen_setup = 1;
		}
	}

	fprintf(stderr, ESC "8");
	tcsetattr(STDERR_FILENO, TCSANOW, &saved);
#endif

	if (screen_cols > 132)
		screen_cols = 132;
}

int screen_width(void)
{
	screen_init();

	if (screen_cols < 1)
		return 80;

	return screen_cols;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
