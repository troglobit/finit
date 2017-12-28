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

#include <ctype.h>
#include <errno.h>
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
#include "log.h"
#include "plugin.h"
#include "private.h"
#include "sig.h"
#include "service.h"
#include "util.h"

static uev_t api_watcher;

static int call(int (*action)(svc_t *), char *buf, size_t len)
{
	return svc_parse_jobstr(buf, len, action, NULL);
}

static int service_stop(svc_t *svc)
{
	if (!svc)
		return 1;

	svc_stop(svc);
	service_step(svc);

	return 0;
}

static int service_start(svc_t *svc)
{
	if (!svc)
		return 1;

	svc_start(svc);
	service_step(svc);

	return 0;
}

static int service_restart(svc_t *svc)
{
	if (!svc)
		return 1;

	svc_mark_dirty(svc);
	service_step(svc);

	return 0;
}

static int do_start  (char *buf, size_t len) { return call(service_start,   buf, len); }
static int do_stop   (char *buf, size_t len) { return call(service_stop,    buf, len); }
static int do_restart(char *buf, size_t len) { return call(service_restart, buf, len); }

static char query_buf[368];
static int missing(char *job, int id)
{
	char buf[20];
	char idstr[10] = "";

	if (!job)
		job = "";
	if (id > 1)
		snprintf(idstr, sizeof(idstr), ":%d", id);

	snprintf(buf, sizeof(buf), "%s%s ", job, idstr);
	strlcat(query_buf, buf, sizeof(query_buf));

	return 1;
}

static int do_query(struct init_request *rq, size_t len)
{
	memset(query_buf, 0, sizeof(query_buf));
	if (svc_parse_jobstr(rq->data, strlen(rq->data) + 1, NULL, missing)) {
		memcpy(rq->data, query_buf, sizeof(rq->data));
		return 1;
	}

	return 0;
}

static svc_t *do_find(char *buf, size_t len)
{
	int id = 1;
	char *ptr, *input;

	input = sanitize(buf, len);
	if (!input)
		return NULL;

	ptr = strchr(input, ':');
	if (ptr) {
		*ptr++ = 0;
		id = atonum(ptr);
	}

	if (isdigit(input[0]))
		return svc_find_by_jobid(atonum(input), id);

	return svc_find_by_nameid(input, id);
}

#ifdef INETD_ENABLED
static int do_query_inetd(char *buf, size_t len)
{
	svc_t *svc;

	svc = do_find(buf, len);
	if (!svc || !svc_is_inetd(svc))
		return 1;

	return inetd_filter_str(&svc->inetd, buf, len);
}
#endif /* INETD_ENABLED */

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

static void send_svc(int sd, svc_t *svc)
{
	svc_t empty;
	size_t len;

	if (!svc) {
		empty.pid = -1;
		svc = &empty;
	}

	len = write(sd, svc, sizeof(*svc));
	if (len != sizeof(*svc))
		_d("Failed sending svc_t to client");
}


/*
 * In contrast to the SysV compat handling in plugins/initctl.c, when
 * `initctl runlevel 0` is issued we default to POWERDOWN the system
 * instead of just halting.
 */
static void api_cb(uev_t *w, void *arg, int events)
{
	int sd, lvl;
	svc_t *svc;
	static svc_t *iter = NULL;
	struct init_request rq;

	sd = accept(w->fd, NULL, NULL);
	if (sd < 0) {
		_pe("Failed serving API request");
		goto error;
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
				/* fallthrough */

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
			_d("debug");
			log_debug();
			break;

		case INIT_CMD_RELOAD: /* 'init q' and 'initctl reload' */
			_d("reload");
			service_reload_dynamic();
			break;

		case INIT_CMD_START_SVC:
			_d("start %s", rq.data);
			result = do_start(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_STOP_SVC:
			_d("stop %s", rq.data);
			result = do_stop(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RESTART_SVC:
			_d("restart %s", rq.data);
			result = do_restart(rq.data, sizeof(rq.data));
			break;

#ifdef INETD_ENABLED
		case INIT_CMD_QUERY_INETD:
			_d("query inetd");
			result = do_query_inetd(rq.data, sizeof(rq.data));
			break;
#endif

		case INIT_CMD_EMIT:
			_d("emit %s", rq.data);
			result = do_handle_emit(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_GET_RUNLEVEL:
			_d("get runlevel");
			rq.runlevel  = runlevel;
			rq.sleeptime = prevlevel;
			break;

		case INIT_CMD_ACK:
			_d("Client failed reading ACK");
			goto leave;

		case INIT_CMD_WDOG_HELLO:
			_d("wdog hello");
			if (rq.runlevel <= 0) {
				result = 1;
				break;
			}

			if (wdogpid > 0 && wdogpid != rq.runlevel) {
				_d("Sending SIGTERM to %d", wdogpid);
				kill(wdogpid, SIGTERM);
				do_sleep(1);
			}
			_d("wdog was %d, now %d is in charge", wdogpid, rq.runlevel);
			wdogpid = rq.runlevel;
			break;

		case INIT_CMD_SVC_ITER:
			_d("svc iter, first: %d", rq.runlevel);
			/*
			 * XXX: This severly limits the number of
			 * simultaneous client connections, but will
			 * have to do for now.
			 */
			svc = svc_iterator(&iter, rq.runlevel);
			send_svc(sd, svc);
			goto leave;

		case INIT_CMD_SVC_QUERY:
			_d("svc query: %s", rq.data);
			result = do_query(&rq, len);
			break;

		case INIT_CMD_SVC_FIND:
			_d("svc find: %s", rq.data);
			send_svc(sd, do_find(rq.data, sizeof(rq.data)));
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
	if (UEV_ERROR == events)
		goto error;
	return;
error:
	api_exit();
	if (api_init(w->ctx))
		_e("Unrecoverable error on API socket");
}

int api_init(uev_ctx_t *ctx)
{
	int sd;
	mode_t oldmask;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	_d("Setting up external API socket ...");
	sd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (-1 == sd) {
		_pe("Failed starting external API socket");
		return 1;
	}

	erase(INIT_SOCKET);
	oldmask = umask(0077);
	if (-1 == bind(sd, (struct sockaddr*)&sun, sizeof(sun)))
		goto error;

	if (-1 == listen(sd, 10))
		goto error;

	umask(oldmask);
	if (!uev_io_init(ctx, &api_watcher, api_cb, NULL, sd, UEV_READ))
		return 0;

error:
	_pe("Failed intializing API socket");
	umask(oldmask);
	close(sd);
	return 1;
}

int api_exit(void)
{
	if (runlevel == 0 || runlevel == 6)
		return close(api_watcher.fd);

	return  shutdown(api_watcher.fd, SHUT_RDWR) ||
		close(api_watcher.fd);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
