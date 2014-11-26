/* Optional /dev/initctl FIFO monitor
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2014  Joachim Nilsson <troglobit@gmail.com>
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
#include "finit.h"

static void parse(void *arg, int fd, int events);

static plugin_t plugin = {
	.io = {
		.cb    = parse,
		.flags = PLUGIN_IO_READ,
	},
};

static void fifo_open(void)
{
	plugin.io.fd = open(FINIT_FIFO, O_RDONLY | O_NONBLOCK);
	if (-1 == plugin.io.fd) {
		_e("Failed opening %s FIFO, error %d: %s", FINIT_FIFO, errno, strerror(errno));
		return;
	}
}

/* Standard reboot/shutdown utilities talk to init using /dev/initctl.
 * We should check if the fifo was recreated and reopen it.
 */
static void parse(void *UNUSED(arg), int fd, int UNUSED(events))
{
	struct init_request rq;

	_d("Receiving request on %s...", FINIT_FIFO);
	while (1) {
		ssize_t len = read(fd, &rq, sizeof(rq));

		if (len <= 0) {
			if (-1 == len) {
				if (EINTR == errno)
					continue;

				if (EAGAIN == errno)
					break;

				_e("Failed reading initctl request, error %d: %s", errno, strerror(errno));
			}
			break;
		}

		if (rq.magic != INIT_MAGIC || len != sizeof(rq)) {
			_e("Invalid initctl request.");
			break;
		}

		_d("Magic OK...");
		if (rq.cmd == INIT_CMD_RUNLVL) {
			switch (rq.runlevel) {
			case '0':
				_d("Halting system (SIGUSR2)");
				do_shutdown(SIGUSR2);
				break;

			case 's':
			case 'S':
				_d("Cannot enter bootstrap after boot ...");
				rq.runlevel = '1';
				/* Fall through to regular processing */

			case '1'...'5':
			case '7'...'9':
				_d("Setting new runlevel %c", rq.runlevel);
				svc_runlevel(rq.runlevel - '0');
				break;

			case '6':
				_d("Rebooting system (SIGUSR1)");
				do_shutdown(SIGUSR1);
				break;

			default:
				_d("Unsupported runlevel: %d", rq.runlevel);
				break;
			}
		} else if (rq.cmd == INIT_CMD_DEBUG) {
			debug = !debug;
		} else {
			_d("Unsupported cmd: %d", rq.cmd);
		}
	}

	close(fd);
	fifo_open();
}

PLUGIN_INIT(plugin_init)
{
	_d("Setting up %s", FINIT_FIFO);
	makefifo(FINIT_FIFO, 0600);

	fifo_open();
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
