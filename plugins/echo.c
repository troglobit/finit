/* Optional inetd plugin for the Echo Protocol, RFC 862
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

#include <arpa/inet.h>
#include <unistd.h>		/* STDIN_FILENO */
#include <sys/socket.h>

#include "../plugin.h"

static int cb(int type)
{
	int sd = STDIN_FILENO;
	char buf[BUFSIZ];
	ssize_t len;
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	len = recvfrom(sd, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr *)&sa, &sa_len);
	if (-1 == len)
		return -1;	/* On error, close connection. */

	return sendto(sd, buf, len, MSG_DONTWAIT, (struct sockaddr *)&sa, sa_len);
}

static plugin_t plugin = {
	.name  = "echo",	/* Must match the inetd /etc/services entry */
	.inetd = {
		.cmd = cb
	},
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
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
