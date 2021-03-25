/* External client API, using UNIX domain socket.
 *
 * Copyright (c) 2015-2021  Joachim Wiberg <troglobit@gmail.com>
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

extern svc_t *wdog;
static uev_t api_watcher;

static int call(int (*action)(svc_t *), char *buf, size_t len)
{
	return svc_parse_jobstr(buf, len, action, NULL);
}

static int stop(svc_t *svc)
{
	if (!svc)
		return 1;

	svc_stop(svc);
	service_step(svc);

	return 0;
}

static int start(svc_t *svc)
{
	if (!svc)
		return 1;

	svc_start(svc);
	service_step(svc);

	return 0;
}

static int restart(svc_t *svc)
{
	if (!svc)
		return 1;

	if (svc_is_blocked(svc))
		svc_start(svc);

	svc_mark_dirty(svc);
	service_step(svc);

	return 0;
}

static int do_start  (char *buf, size_t len) { return call(start,   buf, len); }
static int do_stop   (char *buf, size_t len) { return call(stop,    buf, len); }
static int do_restart(char *buf, size_t len) { return call(restart, buf, len); }

static char query_buf[368];
static int missing(char *job, char *id)
{
	char buf[20];

	if (!job)
		job = "";
	if (!id)
		id = "";

	snprintf(buf, sizeof(buf), "%s:%s ", job, id);
	strlcat(query_buf, buf, sizeof(query_buf));

	return 1;
}

static int do_query(char *buf, size_t len)
{
	memset(query_buf, 0, sizeof(query_buf));
	if (svc_parse_jobstr(buf, len, NULL, missing)) {
		memcpy(buf, query_buf, len);
		return 1;
	}

	return 0;
}

static svc_t *do_find(char *buf, size_t len)
{
	char *id = NULL;
	char *ptr, *input;
	svc_t *iter = NULL;

	input = sanitize(buf, len);
	if (!input)
		return NULL;

	ptr = strchr(input, ':');
	if (ptr) {
		*ptr++ = 0;
		id = ptr;
	}

	if (isdigit(input[0])) {
		char *ep;
		long job = 0;

		errno = 0;
		job = strtol(input, &ep, 10);
		if ((errno == ERANGE && (job == LONG_MAX || job == LONG_MIN)) ||
		    (errno != 0 && job == 0) ||
		    (input == ep))
			return NULL;

		if (!id)
			return svc_job_iterator(&iter, 1, job);

		return svc_find_by_jobid(job, id);
	}

	if (!id)
		return svc_named_iterator(&iter, 1, input);

	return svc_find_by_nameid(input, id);
}

static svc_t *do_find_byc(char *buf, size_t len)
{
	svc_t *svc, *iter = NULL;
	char *input;

	input = sanitize(buf, len);
	if (!input) {
		_d("Invalid input");
		return NULL;
	}

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		char cond[MAX_COND_LEN];

		mkcond(svc, cond, sizeof(cond));
		_d("Comparing input %s with cond %s", input, cond);
		if (string_compare(cond, input))
			return svc;
	}

	_d("None found");
	return NULL;
}

static int do_reboot(int cmd, char *buf, size_t len)
{
	int rc = 1;

	switch (cmd) {
	case INIT_CMD_REBOOT:
		_d("reboot");
		halt = SHUT_REBOOT;
		service_runlevel(6);
		break;

	case INIT_CMD_HALT:
		_d("halt");
		halt = SHUT_HALT;
		service_runlevel(0);
		break;

	case INIT_CMD_POWEROFF:
		_d("poweroff");
		halt = SHUT_OFF;
		service_runlevel(0);
		break;

	case INIT_CMD_SUSPEND:
		_d("suspend");
		sync();
		rc = reboot(RB_SW_SUSPEND);
		if (rc) {
			if (errno == EINVAL)
				snprintf(buf, len, "Kernel does not support suspend.");
			else
				snprintf(buf, len, "Failed: %s", strerror(errno));
		}
		break;

	default:
		rc = 255;
		break;
	}

	return rc;
}

typedef struct {
	char *event;
	void (*cb)(void);
} ev_t;

ev_t ev_list[] = {
	{ "RELOAD", service_reload_dynamic   },
	{ NULL, NULL }
};

static void send_svc(int sd, svc_t *svc)
{
	svc_t empty = { 0 };
	size_t len;

	if (!svc) {
		empty.pid = -1;
		svc = &empty;
	}

	len = write(sd, svc, sizeof(*svc));
	if (len != sizeof(*svc))
		_d("Failed sending svc_t to client");
}

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
			strterm(rq.data, sizeof(rq.data));
			result = do_start(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_STOP_SVC:
			_d("stop %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_stop(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RESTART_SVC:
			_d("restart %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_restart(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_GET_RUNLEVEL:
			_d("get runlevel");
			rq.runlevel  = runlevel;
			rq.sleeptime = prevlevel;
			break;

		case INIT_CMD_REBOOT:
		case INIT_CMD_HALT:
		case INIT_CMD_POWEROFF:
		case INIT_CMD_SUSPEND:
			result = do_reboot(rq.cmd, rq.data, sizeof(rq.data));
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

			_e("Request to hand-over wdog ... to PID %d", rq.runlevel);
			if (!svc_find_by_pid(rq.runlevel)) {
				logit(LOG_ERR, "Cannot find PID %d, not registered.", rq.runlevel);
				break;
			}

			/* Disable and allow Finit to collect bundled watchdog */
			if (wdog) {
				logit(LOG_NOTICE, "Stopping and removing %s (PID:%d)", wdog->cmd, wdog->pid);
				stop(wdog);
				if (wdog->protect) {
					wdog->protect = 0;
					wdog->runlevels = 0;
				}
			}
			break;

		case INIT_CMD_SVC_ITER:
//			_d("svc iter, first: %d", rq.runlevel);
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
			strterm(rq.data, sizeof(rq.data));
			result = do_query(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_SVC_FIND:
			_d("svc find: %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			send_svc(sd, do_find(rq.data, sizeof(rq.data)));
			goto leave;

		case INIT_CMD_SVC_FIND_BYC:
			_d("svc find by cond: %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			send_svc(sd, do_find_byc(rq.data, sizeof(rq.data)));
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
	sd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
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
	uev_io_stop(&api_watcher);

	return close(api_watcher.fd);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
