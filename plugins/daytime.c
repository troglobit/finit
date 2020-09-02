/* Optional inetd plugin for Daytime Protocol, RFC 867
 *
 * Copyright (c) 2016-2020  Joachim Wiberg <troglobit@gmail.com>
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

#define NAME "daytime"


static char *daytime(char *buf, size_t len)
{
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);

	memset(buf, 0, len);
	strftime(buf, len, "%a, %B %d, %Y %T-%Z", tmp);

	return buf;
}

static int recv_peer(int sd, char *buf, ssize_t len, struct sockaddr *sa, socklen_t *sa_len)
{
	len = recvfrom(sd, buf, sizeof(buf), MSG_DONTWAIT, sa, sa_len);
	if (-1 == len)
		return -1;	/* On error, close connection. */

	if (inetd_check_loop(sa, *sa_len, NAME))
		return -1;

	return 0;
} 

static int send_peer(int sd, char *buf, ssize_t len, struct sockaddr *sa, socklen_t sa_len)
{
	char *now = daytime(buf, len);

	return sendto(sd, now, strlen(now), MSG_DONTWAIT, sa, sa_len);
}

static int cb(int type)
{
	int sd = STDIN_FILENO;
	char buf[BUFSIZ];
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (recv_peer(sd, buf, sizeof(buf), (struct sockaddr *)&sa, &sa_len))
		return -1;

	return send_peer(sd, buf, sizeof(buf), (struct sockaddr *)&sa, sa_len);
}

static plugin_t plugin = {
	.name  = NAME,		/* Must match the inetd /etc/services entry */
	.inetd = {
		.cmd = cb
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
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
