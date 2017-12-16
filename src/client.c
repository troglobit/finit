/* External client API, using UNIX domain socket.
 *
 * Copyright (c) 2015-2017  Joachim Nilsson <troglobit@gmail.com>
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
#include <sys/socket.h>
#include <sys/un.h>

#include "client.h"

static int sd = -1;

int client_connect(void)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sd)
		goto error;

	if (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1) {
		close(sd);
		goto error;
	}

	return sd;
error:
	perror("Failed connecting to finit");
	return -1;
}

int client_disconnect(void)
{
	int rc;

	if (sd < 0) {
		errno = EINVAL;
		return -1;
	}

	rc = close(sd);
	sd = -1;

	return rc;
}

int client_send(struct init_request *rq, ssize_t len)
{
	int sd, result = 255;

	sd = client_connect();
	if (-1 == sd)
		return -1;

	if (write(sd, rq, len) != len)
		goto error;

	if (read(sd, rq, len) != len)
		goto error;

	result = 0;
	goto exit;
error:
	perror("Failed communicating with finit");
exit:
	client_disconnect();
	return result;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
