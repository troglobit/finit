/* Optional inetd plugin for Time Protocol (rdate), RFC 868
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

#define NAME "time"

/* UNIX epoch starts midnight, 1st Jan, 1970 */
#define EPOCH_OFFSET 2208988800ULL

/* Return number of seconds since midnight, 1st Jan, 1900 */
static char *rfctime(char *buf, size_t *len)
{
	time_t now;

	now = time(NULL);
	if ((time_t)-1 == now)
		return NULL;

	/*
	 * Account for UNIX epoch offset, and
	 * convert to network byte order
	 */
	now += EPOCH_OFFSET;
	now  = htonl(now);

	*len = sizeof(now);
	memcpy(buf, &now, *len);

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
	char *now;

	now = rfctime(buf, (size_t *)&len);
	if (!now)
		return -1;	/* On error, close connection. */

	return sendto(sd, now, len, MSG_DONTWAIT, sa, sa_len);
}

static int cb(int UNUSED(type))
{
	int sd = STDIN_FILENO;
	char buf[BUFSIZ];
	struct sockaddr_storage sa;
	socklen_t sa_len = sizeof(sa);

	if (recv_peer(sd, buf, sizeof(buf), (struct sockaddr *)&sa, &sa_len))
		return -1;	/* On error, close connection. */

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
