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

/* Allowed characters in job/id/name */
static int isallowed(int ch)
{
	return isalnum(ch) || isspace(ch) || ch == ':';
}

/* Sanitize user input, make sure to NUL terminate. */
static char *sanitize(char *arg, size_t len)
{
	size_t i = 0;

	while (isallowed(arg[i]) && i < len)
		i++;

	if (i + 1 < len) {
		arg[i + 1] = 0;
		return arg;
	}

	return NULL;
}

static int call(int (*action)(svc_t *), char *buf, size_t len)
{
	int result = 0;
	char *input, *token, *pos;

	input = sanitize(buf, len);
	if (!input)
		return -1;

	token = strtok_r(input, " ", &pos);
	while (token) {
		svc_t *svc;

		if (isdigit(token[0])) {
			int job = atonum(token);
			char *ptr = strchr(token, ':');

			if (!ptr) {
				svc = svc_job_iterator(1, job);
				while (svc) {
					result += action(svc);
					svc = svc_job_iterator(0, job);
				}
			} else {
				*ptr++ = 0;
				job = atonum(token);
				result += action(svc_find_by_jobid(job, atonum(ptr)));
			}

		} else {
			svc = svc_named_iterator(1, token);
			while (svc) {
				result += action(svc);
				svc = svc_named_iterator(0, token);
			}
		}

		token = strtok_r(NULL, " ", &pos);
	}

	return result;
}

static int do_start  (char *buf, size_t len) { return call(service_start,   buf, len); }
static int do_stop   (char *buf, size_t len) { return call(service_stop,    buf, len); }
static int do_reload (char *buf, size_t len) { return call(service_reload,  buf, len); }
static int do_restart(char *buf, size_t len) { return call(service_restart, buf, len); }

static int do_query_inetd(char *buf, size_t len)
{
	char *input = sanitize(buf, len);
	svc_t *svc;

	if (!input)
		return -1;

	svc = svc_find_by_jobid(atonum(input), 1);
	if (!svc || !svc_is_inetd(svc)) {
		_e("Cannot %s svc %s ...", !svc ? "find" : "query, not an inetd", input);
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

		len = read(sd, &rq, sizeof(rq));
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
