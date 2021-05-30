/* Reset terminal line settings (stty sane)
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

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>	      /* _POSIX_VDISABLE */
#include <sys/ttydefaults.h>  /* Not included by default in musl libc */
#include <termios.h>

#include "helpers.h"
#include "sig.h"

/* B0 means; keep kernel default */
speed_t stty_parse_speed(char *baud)
{
	char *ptr;
	size_t i;
	unsigned long val;
	struct { unsigned long val; speed_t speed; } v2s[] = {
		{       0, B0       },
		{      50, B50      },
		{      75, B75      },
		{     110, B110     },
		{     134, B134     },
		{     150, B150     },
		{     200, B200     },
		{     300, B300     },
		{     600, B600     },
		{    1200, B1200    },
		{    1800, B1800    },
		{    2400, B2400    },
		{    4800, B4800    },
		{    9600, B9600    },
		{   19200, B19200   },
		{   38400, B38400   },
		{   57600, B57600   },
		{  115200, B115200  },
		{  230400, B230400  },
		{  460800, B460800  },
		{  500000, B500000  },
		{  576000, B576000  },
		{  921600, B921600  },
		{ 1000000, B1000000 },
		{ 1152000, B1152000 },
		{ 1500000, B1500000 },
		{ 2000000, B2000000 },
#ifdef B4000000
		{ 2500000, B2500000 },
		{ 3000000, B3000000 },
		{ 3500000, B3500000 },
		{ 4000000, B4000000 },
#endif
	};

	if (!baud || !baud[0])
		return B0;

	errno = 0;
	val = strtoul(baud, &ptr, 10);
	if (errno || ptr == baud)
		return B0;

	for (i = 0; i < sizeof(v2s) / sizeof(v2s[0]); i++) {
		if (v2s[i].val == val)
			return v2s[i].speed;
	}

	return B0;
}

void stty(int fd, speed_t speed)
{
	struct termios term;
	struct sigaction sa;

	/* Ignore SIGINT during login phase, /bin/login resets */
	IGNSIG(sa, SIGINT, SA_RESTART);

	tcdrain(fd);
	if (tcgetattr(fd, &term))
		return;

	if (speed != B0) {
		cfsetispeed(&term, speed);
		cfsetospeed(&term, speed);
	}

	/* Modem specific control flags */
	term.c_cflag     &= ~(PARENB|PARODD|CSTOPB|CRTSCTS);
	term.c_cflag     |= CS8|HUPCL|CREAD|CLOCAL;
	term.c_line       = 0;

	/* Timeouts, minimum chars and default flags */
	term.c_cc[VTIME]  = 0;
	term.c_cc[VMIN]   = 1;
	term.c_iflag      = ICRNL|IXON|IXOFF|IUTF8;
	term.c_oflag      = OPOST|ONLCR;
	term.c_lflag     |= ICANON|ISIG|ECHO|ECHOE|ECHOK|ECHOKE;

	/* Reset special characters to defaults */
	term.c_cc[VINTR]  = CINTR;
	term.c_cc[VQUIT]  = CQUIT;
	term.c_cc[VEOF]   = CEOF;
	term.c_cc[VEOL]   = CEOL;
	term.c_cc[VKILL]  = CKILL;
	term.c_cc[VERASE] = CERASE;
	tcsetattr(fd, TCSAFLUSH, &term);

	/* Show cursor again, if it was hidden previously */
	dprint(fd, "\033[?25h", 6);

	/* Enable line wrap, if disabled previously */
	dprint(fd, "\033[7h", 4);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
