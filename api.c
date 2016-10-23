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
#include <lite/lite.h>
#include <uev/uev.h>

#include "config.h"
#include "finit.h"
#include "cond.h"
#include "conf.h"
#include "helpers.h"
#include "plugin.h"
#include "sig.h"
#include "service.h"

uev_t api_watcher;

/* Allowed characters in job/id/name */
static int isallowed(int ch)
{
	return isprint(ch);
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
		char *ptr = strchr(token, ':');

		if (isdigit(token[0])) {
			int job = atonum(token);

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
			if (!ptr) {
				svc = svc_named_iterator(1, token);
				while (svc) {
					result += action(svc);
					svc = svc_named_iterator(0, token);
				}
			} else {
				*ptr++ = 0;
				result += action(svc_find_by_nameid(token, atonum(ptr)));
			}
		}

		token = strtok_r(NULL, " ", &pos);
	}

	return result;
}

static int service_block(svc_t *svc)
{
	svc_block(svc);
	service_step(svc);

	return 0;
}

static int service_unblock(svc_t *svc)
{
	svc_unblock(svc);
	service_step(svc);

	return 0;
}

static int service_restart(svc_t *svc)
{
	svc_mark_dirty(svc);
	service_step(svc);
	return 0;
}

static int do_start  (char *buf, size_t len) { return call(service_unblock, buf, len); }
static int do_pause  (char *buf, size_t len) { return call(service_block,   buf, len); }
static int do_restart(char *buf, size_t len) { return call(service_restart, buf, len); }

#ifndef INETD_DISABLED
static int do_query_inetd(char *buf, size_t len)
{
	int id = 1;
	char *ptr, *input = sanitize(buf, len);
	svc_t *svc;

	if (!input)
		return -1;

	ptr = strchr(input, ':');
	if (ptr) {
		*ptr++ = 0;
		id = atonum(ptr);
	}

	svc = svc_find_by_jobid(atonum(input), id);
	if (!svc || !svc_is_inetd(svc)) {
		_e("Cannot %s svc %s ...", !svc ? "find" : "query, not an inetd", input);
		return 1;
	}

	return inetd_filter_str(&svc->inetd, buf, len);
}
#endif /* !INETD_DISABLED */

typedef struct {
	char *event;
	void (*cb)(void);
} ev_t;

ev_t ev_list[] = {
	{ "RELOAD", service_reload_dynamic   },
	{ NULL, NULL }
};

static int do_handle_event(char *event)
{
	int i;

	for (i = 0; ev_list[i].event; i++) {
		ev_t *e = &ev_list[i];
		size_t len = MAX(strlen(e->event), strlen(event));

		if (!strncasecmp(e->event, event, len)) {
			e->cb();
			return 0;
		}
	}

	if (event[0] == '-')
		cond_clear(&event[1]);
	else if (event[0] == '+')
		cond_set(&event[1]);
	else
		cond_set(event);
	return 0;
}

static int do_handle_emit(char *buf, size_t len)
{
	int result = 0;
	char *input, *event, *pos;

	input = sanitize(buf, len);
	if (!input)
		return -1;

	event = strtok_r(input, " ", &pos);
	while (event) {
		result += do_handle_event(event);
		event = strtok_r(NULL, " ", &pos);
	}

	return result;
}

/*
 * In contrast to the SysV compat handling in plugins/initctl.c, when
 * `initctl runlevel 0` is issued we default to POWERDOWN the system
 * instead of just halting.
 */
static void cb(uev_t *w, void *UNUSED(arg), int UNUSED(events))
{
	int sd, lvl;
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
			_e("Invalid initctl request");
			break;
		}

		switch (rq.cmd) {
		case INIT_CMD_RUNLVL:
			switch (rq.runlevel) {
			case 's':
			case 'S':
				rq.runlevel = '1'; /* Single user mode */
				/* Fall through to regular processing */

			case '0'...'9':
				_d("Setting new runlevel %c", rq.runlevel);
				lvl = rq.runlevel - '0';
				if (lvl == 0)
					halt = SHUT_OFF;
				if (lvl == 6)
					halt = SHUT_REBOOT;
				service_runlevel(lvl);
				break;

			default:
				_d("Unsupported runlevel: %d", rq.runlevel);
				break;
			}
			break;

		case INIT_CMD_DEBUG:
			debug = !debug;
			if (debug)
				silent = 0;
			else
				silent = quiet ? 1 : SILENT_MODE;
			break;

		case INIT_CMD_RELOAD: /* 'init q' and 'initctl reload' */
			service_reload_dynamic();
			break;

		case INIT_CMD_START_SVC:
			result = do_start(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_STOP_SVC:
			result = do_pause(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RESTART_SVC:
			result = do_restart(rq.data, sizeof(rq.data));
			break;

#ifndef INETD_DISABLED
		case INIT_CMD_QUERY_INETD:
			result = do_query_inetd(rq.data, sizeof(rq.data));
			break;
#endif

		case INIT_CMD_EMIT:
			result = do_handle_emit(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_GET_RUNLEVEL:
			rq.runlevel = runlevel;
			break;

		case INIT_CMD_ACK:
			_d("Client failed reading ACK");
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
			_d("Failed sending ACK/NACK back to client");
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

	sd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == sd) {
		_pe("Failed starting external API socket");
		return 1;
	}

	erase(INIT_SOCKET);
	if (-1 == bind(sd, (struct sockaddr*)&sun, sizeof(sun)))
		goto error;

	if (-1 == listen(sd, 10))
		goto error;

	if (!uev_io_init(ctx, &api_watcher, cb, NULL, sd, UEV_READ))
		return 0;

error:
	_pe("Failed intializing API socket");
	close(sd);
	return 1;
}

int api_exit(void)
{
	return  shutdown(api_watcher.fd, SHUT_RDWR) ||
		close(api_watcher.fd);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
