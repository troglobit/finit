/* Optional inetd plugin for time (rdate) protocol RFC 868
 *
 * Copyright (c) 2015  Joachim Nilsson <troglobit@gmail.com>
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

#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>		/* STDIN_FILENO */
#include <sys/socket.h>

#include "plugin.h"

/* UNIX epoch starts midnight, 1st Jan, 1970 */
#define EPOCH_OFFSET 2208988800ULL

/* Return number of seconds since midnight, 1st Jan, 1900 */
static int rfc868(int type)
{
	int sd = STDIN_FILENO;
	time_t now;
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (SOCK_DGRAM == type) {
		int dummy;

		/* Read empty datagram in UDP mode before sending reply. */
		if (-1 == recvfrom(sd, &dummy, sizeof(dummy), MSG_DONTWAIT, (struct sockaddr *)&sa, &sa_len))
			return -1;	/* On error, close connection. */
	}

	now = time(NULL);
	if ((time_t)-1 == now)
		return -1;		/* On error, close connection. */

	/* Account for UNIX epoch offset */
	now += EPOCH_OFFSET;

	/* Convert to network byte order, not explicitly stated in RFC */
	now = htonl(now);

	/* Send reply, ignore error, simply close connection. */
	return sendto(sd, &now, sizeof(now), MSG_DONTWAIT, (struct sockaddr *)&sa, sa_len);
}

static plugin_t plugin = {
	.name  = "time",
	.inetd = {
		.cmd = rfc868
	}
};

PLUGIN_INIT(plugin_init)
{
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
