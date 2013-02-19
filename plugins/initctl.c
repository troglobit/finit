/* Optional /dev/initctl FIFO monitor
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
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
#include <fcntl.h>		/* O_RDONLY et al */
#include <signal.h>
#include <string.h>
#include <unistd.h>		/* read() */
#include <sys/stat.h>

#include "helpers.h"
#include "plugin.h"
#include "sig.h"

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
static void read_initctl(void *UNUSED(arg), int fd, int UNUSED(events))
{
	struct init_request request;

	_d("Receiving request on %s...", FINIT_FIFO);
	while (1) {
		ssize_t len = read(fd, &request, sizeof(request));

		if (-1 == len) {
			_d("Error %d", errno);
			break;	/* Probably EAGAIN */
		}

		if (request.magic != INIT_MAGIC)
			continue;

		_d("Magic OK...");
		if (request.cmd == INIT_CMD_RUNLVL) {
			switch (request.runlevel) {
			case '0':
				_d("Halting system (SIGUSR2)");
				do_shutdown(SIGUSR2);
				break;

			case '6':
				_d("Rebooting system (SIGUSR1)");
				do_shutdown(SIGUSR1);
				break;

			default:
				_d("Unsupported runlevel: %d", request.runlevel);
				break;
			}
		} else {
			_d("Unsupported cmd: %d", request.cmd);
		}
	}
}

static plugin_t plugin = {
	.io = {
		.cb    = read_initctl,
		.flags = PLUGIN_IO_READ,
	},
};

PLUGIN_INIT(plugin_init)
{
	_d("Setting up %s", FINIT_FIFO);
	mkfifo(FINIT_FIFO, 0600);

	plugin.io.fd = open(FINIT_FIFO, O_RDONLY | O_NONBLOCK);
	if (-1 == plugin.io.fd) {
		_e("Failed opening %s FIFO, error %d: %s", FINIT_FIFO, errno, strerror(errno));
		return;
	}

	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
