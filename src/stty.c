/* Reset terminal line settings (stty sane)
 *
 * Copyright (c) 2016-2017  Joachim Nilsson <troglobit@gmail.com>
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

#include <unistd.h>	      /* _POSIX_VDISABLE */
#include <sys/ttydefaults.h>  /* Not included by default in musl libc */
#include <termios.h>

void stty(int fd, speed_t speed)
{
	struct termios term;

	tcdrain(fd);
	if (tcgetattr(fd, &term))
		return;

	cfsetispeed(&term, speed);
	cfsetospeed(&term, speed);
	tcsetattr(fd, TCSAFLUSH, &term);
	tcflush(fd, TCIOFLUSH);

	/* Disable modem specific flags */
	term.c_cflag     &= ~(0|CSTOPB|PARENB|PARODD);
	term.c_cflag     &= ~CRTSCTS;
	term.c_cflag     |= CLOCAL;

	/* Timeouts, minimum chars and default flags */
	term.c_cc[VTIME]  = 0;
	term.c_cc[VMIN]   = 1;
	term.c_iflag      = ICRNL|IXON|IXOFF;
	term.c_oflag      = OPOST|ONLCR;
	term.c_cflag     |= CS8|CREAD|HUPCL|CBAUDEX;
	term.c_lflag     |= ICANON|ISIG|ECHO|ECHOE|ECHOK|ECHOKE;

	/* Reset special characters to defaults */
	term.c_cc[VINTR]  = CTRL('C');
	term.c_cc[VQUIT]  = CTRL('\\');
	term.c_cc[VEOF]   = CTRL('D');
	term.c_cc[VEOL]   = '\n';
	term.c_cc[VKILL]  = CTRL('U');
	term.c_cc[VERASE] = CERASE;
	tcsetattr(fd, TCSANOW, &term);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
