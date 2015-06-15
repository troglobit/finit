/* External client API, using UNIX domain socket.
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

#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "helpers.h"
#include "plugin.h"
#include "sig.h"
#include "finit.h"
#include "service.h"

#include "libite/lite.h"
#include "libuev/uev.h"

uev_t api_watcher;

/* Make sure the job id is NUL terminated and on the form SVC:ID. */
static char *sanitize_jobid(char *jobid, size_t len)
{
	size_t i = 0;
	static char buf[42];

	while (isdigit(jobid[i]) && i < len)
		i++;

	if (jobid[i++] == ':') {
		while (isdigit(jobid[i]) && i < len)
			i++;
	}

	if (i + 1 < sizeof(buf)) {
		memcpy(buf, jobid, i);
		buf[i + 1] = 0;

		return buf;
	}

	return NULL;
}

static svc_t *convert_jobid(int noid, char *jobid, size_t len)
{
	int job, id = 1;
	char *token, *ptr;

	jobid = sanitize_jobid(jobid, len);
	if (!jobid)
		return NULL;

	if (noid && (ptr = strchr(jobid, ':')))
	    ptr = 0;

	token = strtok(jobid, ":");
	if (!token) {
		job = atoi(jobid);
	} else {
		job = atoi(token);
		token = strtok(NULL, "");
		if (token)
			id = atoi(token);
	}

	_d("Got job %d id %d ...", job, id);
	return svc_find_by_jobid(job, id);
}

static int do_start(char *buf, size_t len)
{
	return service_start(convert_jobid(0, buf, len));
}

static int do_stop(char *buf, size_t len)
{
	return service_stop(convert_jobid(0, buf, len));
}

static int do_reload(char *buf, size_t len)
{
	return service_reload(convert_jobid(0, buf, len));
}

static int do_restart(char *buf, size_t len)
{
	return service_restart(convert_jobid(0, buf, len));
}

static int do_query_inetd(char *buf, size_t len)
{
	svc_t *svc;

	svc = convert_jobid(1, buf, len);
	if (!svc || !svc_is_inetd(svc)) {
		_e("Cannot %s svc %s ...", !svc ? "find" : "query, not an inetd", buf);
		return 1;
	}

	return inetd_filter_str(&svc->inetd, buf, len);
}

static void cb(uev_t *w, void *UNUSED(arg), int UNUSED(events))
{
	int sd;
	struct init_request rq;

	sd = accept(w->fd, NULL, NULL);
	if (sd < 0) {
		_pe("Failed serving API request");
		return;
	}

	while (1) {
		int result = 0;
		ssize_t len;

		memset(&rq, 0, sizeof(rq));
		len = read(sd, &rq, sizeof(rq));
		if (len <= 0) {
			if (-1 == len) {
				if (EINTR == errno)
					continue;

				if (EAGAIN == errno)
					break;

				_e("Failed reading initctl request, error %d: %s", errno, strerror(errno));
			}

			_d("Nothing to do, bailing out.");

			break;
		}

		if (rq.magic != INIT_MAGIC || len != sizeof(rq)) {
			_e("Invalid initctl request.");
			break;
		}

		switch (rq.cmd) {
		case INIT_CMD_RUNLVL:
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
				service_runlevel(rq.runlevel - '0');
				break;

			case '6':
				_d("Rebooting system (SIGUSR1)");
				do_shutdown(SIGUSR1);
				break;

			default:
				_d("Unsupported runlevel: %d", rq.runlevel);
				break;
			}
			break;

		case INIT_CMD_DEBUG:
			debug = !debug;
			break;

		case INIT_CMD_RELOAD:
			reload_finit_d();
			break;

		case INIT_CMD_START_SVC:
			result = do_start(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_STOP_SVC:
			result = do_stop(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RELOAD_SVC:
			result = do_reload(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RESTART_SVC:
			result = do_restart(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_QUERY_INETD:
			result = do_query_inetd(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_ACK:
			_d("Client failed reading ACK.");
			goto leave;

		default:
			_d("Unsupported cmd: %d", rq.cmd);
			break;
		}

		if (result)
			rq.cmd = INIT_CMD_NACK;
		else
			rq.cmd = INIT_CMD_ACK;
		len = write(sd, &rq, sizeof(rq));
		if (len != sizeof(rq))
			_d("Failed sending ACK/NACK back to client.");
#if 0
		else
			sleep(1); /* Give client time to read FIFO */
		_e("Ready for next station.");
#endif
	}

leave:
	close(sd);
}

int api_init(uev_ctx_t *ctx)
{
	int sd;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sd) {
		_pe("Failed starting external API socket");
		return 1;
	}

	erase(INIT_SOCKET);
	if (-1 == bind(sd, (struct sockaddr*)&sun, sizeof(sun)))
		goto error;

	if (-1 == listen(sd, 10))
		goto error;

	return uev_io_init(ctx, &api_watcher, cb, NULL, sd, UEV_READ);

error:
	_pe("Failed intializing API socket");
	close(sd);
	return 1;
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
