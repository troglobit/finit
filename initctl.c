/*
Improved fast init

Copyright (c) 2008 Claudio Matsuoka

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifdef LISTEN_INITCTL
#include <features.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>         /* According to POSIX.1-2001 */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "finit.h"

#define INIT_MAGIC		0x03091969
#define INIT_CMD_RUNLVL		1

struct init_request {
	int	magic;		/* Magic number			*/
	int	cmd;		/* What kind of request		*/
	int	runlevel;	/* Runlevel to change to	*/
	int	sleeptime;	/* Time between TERM and KILL	*/
	char	data[368];
};

/* Standard reboot/shutdown utilities talk to init using /dev/initctl.
 * We should check if the fifo was recreated and reopen it.
 */
void listen_initctl(void)
{
	if (!fork()) {
		int ctl;
		fd_set fds;
		struct init_request request;

		mkfifo("/dev/initctl", 0600);
		ctl = open("/dev/initctl", O_RDONLY);

		while (1) {
			FD_ZERO(&fds);
			FD_SET(ctl, &fds);
			if (select(ctl + 1, &fds, NULL, NULL, NULL) <= 0)
				continue;

			read(ctl, &request, sizeof(request));

			if (request.magic != INIT_MAGIC)
				continue;

			if (request.cmd == INIT_CMD_RUNLVL) {
				switch (request.runlevel) {
				case '0':
					do_shutdown(SIGUSR2);
					break;
				case '6':
					do_shutdown(SIGUSR1);
				}
			}
		}
	}
}

#endif  /* LISTEN_INITCTL */

/**
 * Local Variables:
 *  version-control: t
 *  c-file-style: "linux"
 * End:
 */
