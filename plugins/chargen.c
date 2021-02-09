/* Optional inetd plugin for the Character Generator Protocol, RFC 864
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

#include <arpa/inet.h>
#include <unistd.h>		/* STDIN_FILENO */
#include <sys/socket.h>

#include "plugin.h"

#define NAME    "chargen"
#define PATTERN "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ "

static char *generator(char *buf, size_t buflen)
{
	size_t num, len, width = 72;
	static size_t pos = 0;
	const char pattern[] = PATTERN;

	if (buflen < width)
		width = buflen;

	len = width;
	if (len + 3 > buflen)
		len = buflen - 3;
	if (pos + len > sizeof(pattern)) {
		num = sizeof(pattern) - pos;
		len -= num;
	} else {
		num = width;
		if (num + 3 > buflen)
			num = buflen - 3;
		len = 0;
	}

	strncpy(buf, &pattern[pos], num--);
	if (len++)
		strncpy(&buf[num], pattern, len);
	strlcat(buf, "\r\n", width);

	if (++pos >= sizeof(pattern) - 1)
		pos = 0;

	return buf;
}

static int recv_peer(int sd, char *buf, ssize_t len, struct sockaddr *sa, socklen_t *sa_len)
{
	len = recvfrom(sd, buf, len, MSG_DONTWAIT, sa, sa_len);
	if (-1 == len)
		return -1;	/* On error, close connection. */

	if (inetd_check_loop(sa, *sa_len, NAME))
		return -1;

	return 0;
} 

static int send_peer(int sd, char *buf, ssize_t len, struct sockaddr *sa, socklen_t sa_len)
{
	char *pattern = generator(buf, len);

	return sendto(sd, pattern, strlen(pattern), MSG_DONTWAIT, sa, sa_len);
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
