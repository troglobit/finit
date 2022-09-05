/* External client API, using UNIX domain socket.
 *
 * Copyright (c) 2015-2022  Joachim Wiberg <troglobit@gmail.com>
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
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "client.h"
#include "log.h"

static int sd = -1;

int client_connect(void)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	sd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if (-1 == sd) {
		warn("Failed creating UNIX domain socket");
		return -1;
	}

	if (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1) {
		if (errno != ENOENT)
			warnx("Failed connecting to finit");
		close(sd);
		return -1;
	}

	return sd;
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
	struct pollfd pfd = { 0 };
	int result = 255;
	int rc;

	if (client_connect() == -1)
		return -1;

	pfd.fd     = sd;
	pfd.events = POLLOUT;
	if (poll(&pfd, 1, 2000) <= 0) {
		warn("Timed out waiting for Finit, errno %d", errno);
		return -1;
	}

	if (write(sd, rq, len) != len) {
		warn("Failed communicating with Finit, errno %d", errno);
		return -1;
	}

	pfd.fd = sd;
	pfd.events = POLLIN | POLLERR | POLLHUP;
	if ((rc = poll(&pfd, 1, 2000)) <= 0) {
		if (rc) {
			if (errno == EINTR) /* shutdown/reboot */
				return -1;
			warn("poll(), errno %d", errno);
		} else
			warnx("Timed out waiting for reply from Finit.");
		return -1;
	}

	if (read(sd, rq, len) != len) {
		warn("Failed reading reply from Finit, errno %d", errno);
		return -1;
	}

	if (rq->cmd == INIT_CMD_NACK)
		result = 1;
	else
		result = 0;
exit:
	client_disconnect();

	return result;
}

svc_t *client_svc_iterator(int first)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd   = INIT_CMD_SVC_ITER,
	};
	static svc_t svc;

	if (client_connect() == -1)
		return NULL;

	if (first)
		rq.runlevel = 1;
	else
		rq.runlevel = 0;

	if (write(sd, &rq, sizeof(rq)) != sizeof(rq))
		goto error;
	if (read(sd, &svc, sizeof(svc)) != sizeof(svc))
		goto error;

	client_disconnect();
	if (svc.pid < 0)
		return NULL;

	return &svc;
error:
	warn("Failed communicating with finit, error %d", errno);
	client_disconnect();
	sd = -1;

	return NULL;
}

static svc_t *do_cmd(int cmd, const char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd   = cmd,
	};
	static svc_t svc;

	if (client_connect() == -1)
		return NULL;

	strlcpy(rq.data, arg, sizeof(rq.data));
	if (write(sd, &rq, sizeof(rq)) != sizeof(rq))
		goto error;
	if (read(sd, &svc, sizeof(svc)) != sizeof(svc))
		goto error;

	client_disconnect();
	if (svc.pid < 0)
		return NULL;

	return &svc;
error:
	client_disconnect();
	warn("Failed communicating with finit, error %d", errno);

	return NULL;
}

svc_t *client_svc_find(const char *arg)
{
	return do_cmd(INIT_CMD_SVC_FIND, arg);
}


svc_t *client_svc_find_by_cond(const char *arg)
{
	return do_cmd(INIT_CMD_SVC_FIND_BYC, arg);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
