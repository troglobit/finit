/* External client API, using UNIX domain socket.
 *
 * Copyright (c) 2015-2025  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"		/* Generated by configure script */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif
#include <uev/uev.h>

#include "finit.h"
#include "cond.h"
#include "conf.h"
#include "helpers.h"
#include "log.h"
#include "plugin.h"
#include "private.h"
#include "schedule.h"
#include "service.h"
#include "sm.h"
#include "sig.h"
#include "util.h"

static uev_t api_watcher;

static int call(int (*action)(svc_t *, void *), char *buf, size_t len)
{
	return svc_parse_jobstr(buf, len, NULL, action, NULL);
}

static int stop(svc_t *svc, void *user_data)
{
	(void)user_data;

	if (!svc)
		return 1;

	service_timeout_cancel(svc);
	svc_stop(svc);
	service_step(svc);
	service_step_all(SVC_TYPE_ANY);

	return 0;
}

static int start(svc_t *svc, void *user_data)
{
	(void)user_data;

	if (!svc)
		return 1;

	service_timeout_cancel(svc);
	svc_start(svc);
	service_step(svc);
	service_step_all(SVC_TYPE_ANY);

	return 0;
}

/*
 * NOTE: this does not wait for svc to be stopped first, that is the
 *       responsibility of initctl to do.  Otherwise we'd block PID 1,
 *       or introduce some nasty race conditions.
 */
static int restart(svc_t *svc, void *user_data)
{
	if (!svc)
		return 1;

	if (!svc_is_running(svc))
		return start(svc, user_data);

	service_timeout_cancel(svc);
	service_stop(svc);
	service_step(svc);

	return 0;
}

static int reload(svc_t *svc, void *user_data)
{
	(void)user_data;

	if (!svc)
		return 1;

	if (svc_is_blocked(svc))
		svc_start(svc);
	else
		service_timeout_cancel(svc);

	svc_mark_dirty(svc);
	service_step(svc);

	return 0;
}

static int do_stop   (char *buf, size_t len) { return call(stop,    buf, len); }
static int do_start  (char *buf, size_t len) { return call(start,   buf, len); }
static int do_restart(char *buf, size_t len) { return call(restart, buf, len); }
static int do_reload (char *buf, size_t len) { return call(reload,  buf, len); }

static int do_signal_svc(svc_t *svc, void *user_data)
{
	int signo;

	if (!svc || !user_data || !svc_is_running(svc))
		return 1;

	signo = *(int *)user_data;
	return !!kill(svc->pid, signo);
}

static int do_signal(char *buf, size_t len, int sig)
{
	/* Sanity check: do we know this signal? */
	if (!*sig2str(sig))
		return 1;

	return svc_parse_jobstr(buf, len, &sig, do_signal_svc, NULL);
}

static char query_buf[368];
static int missing(char *job, char *id, void *user_data)
{
	char buf[20];

	(void)user_data;

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
	query_buf[0] = 0;
	if (svc_parse_jobstr(buf, len, NULL, NULL, missing)) {
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

	return svc_find(input, id);
}

static svc_t *do_find_byc(char *buf, size_t len)
{
	char *input;

	input = sanitize(buf, len);
	if (!input) {
		dbg("Invalid input");
		return NULL;
	}

	return svc_find_by_cond(input);
}

static void bypass_shutdown(void *);
struct wq emergency = { .cb = bypass_shutdown };

static void bypass_shutdown(void *unused)
{
	(void)unused;

	cprintf("TIMEOUT TIMEOUT SHUTTING DOWN NOW!!\n");
	do_shutdown(halt);
}

static int do_reboot(int cmd, int timeout, char *buf, size_t len)
{
	int rc = 1;

	switch (cmd) {
	case INIT_CMD_REBOOT:
		halt = SHUT_REBOOT;
		break;

	case INIT_CMD_HALT:
		halt = SHUT_HALT;
		break;

	case INIT_CMD_POWEROFF:
		halt = SHUT_OFF;
		break;

	case INIT_CMD_SUSPEND:
		break;

	default:
		return 255;
	}

	if (timeout > 0) {
		emergency.delay = timeout * 1000;
		schedule_work(&emergency);
	}

	switch (cmd) {
	case INIT_CMD_REBOOT:
		dbg("reboot");
		halt = SHUT_REBOOT;
		sm_runlevel(6);
		break;

	case INIT_CMD_HALT:
		dbg("halt");
		halt = SHUT_HALT;
		sm_runlevel(0);
		break;

	case INIT_CMD_POWEROFF:
		dbg("poweroff");
		halt = SHUT_OFF;
		sm_runlevel(0);
		break;

	case INIT_CMD_SUSPEND:
		dbg("suspend");
		sync();
		rc = suspend();
		if (rc) {
			if (errno == EINVAL)
				snprintf(buf, len, "Kernel does not support suspend to RAM.");
			else
				snprintf(buf, len, "Failed: %s", strerror(errno));
		}
		break;

	default:
		/* Handled previously */
		break;
	}

	return rc;
}

typedef struct {
	char *event;
	void (*cb)(void);
} ev_t;

ev_t ev_list[] = {
	{ "RELOAD", sm_reload   },
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
		dbg("Failed sending svc_t to client");
}

static void api_cb(uev_t *w, void *arg, int events)
{
	static svc_t *iter = NULL;
	struct init_request rq;
	int sd, lvl;
	svc_t *svc;

	(void)arg;

	if (UEV_ERROR == events) {
		dbg("%s(): api socket %d invalid.", __func__, w->fd);
		goto error;
	}

	/*
	 * TODO: refactor to use accept4() and a new uev handler for the
	 * client socket instead of risking blocking PID 1 if the client
	 * is misbehaving.
	 */
//	sd = accept4(w->fd, NULL, NULL, SOCK_NONBLOCK);
	sd = accept(w->fd, NULL, NULL);
	if (sd < 0) {
		err(1, "Failed serving API request");
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

				/* we get here when client restarts itself */
				if (ECONNRESET == errno)
					break;

				errx(1, "Failed reading initctl request, error %d: %s", errno, strerror(errno));
			}

			break;
		}

		if (rq.magic != INIT_MAGIC || len != sizeof(rq)) {
			errx(1, "Invalid initctl request");
			break;
		}

		switch (rq.cmd) {
		case INIT_CMD_RELOAD:
		case INIT_CMD_START_SVC:
		case INIT_CMD_RESTART_SVC:
		case INIT_CMD_STOP_SVC:
		case INIT_CMD_RELOAD_SVC:
		case INIT_CMD_REBOOT:
		case INIT_CMD_HALT:
		case INIT_CMD_POWEROFF:
		case INIT_CMD_SUSPEND:
			if (IS_RESERVED_RUNLEVEL(runlevel)) {
				strterm(rq.data, sizeof(rq.data));
				warnx("Unsupported command (cmd: %d, data: %s) in runlevel S and 6/0.",
				      rq.cmd, rq.data);
				goto leave;
			}
		default:
			break;
		}

		switch (rq.cmd) {
		case INIT_CMD_RUNLVL:
			/* Allow changing cfglevel in runlevel S */
			if (IS_RESERVED_RUNLEVEL(runlevel)) {
				if (runlevel != INIT_LEVEL) {
					warnx("Cannot abort runlevel 6/0.");
					break;
				}
			}

			switch (rq.runlevel) {
			case 's':
			case 'S':
				rq.runlevel = '1'; /* Single user mode */
				/* fallthrough */

			case '0'...'9':
				dbg("Setting new runlevel %c", rq.runlevel);
				lvl = rq.runlevel - '0';
				if (lvl == 0)
					halt = SHUT_OFF;
				if (lvl == 6)
					halt = SHUT_REBOOT;

				/* User requested change in next runlevel */
				if (runlevel == INIT_LEVEL)
					cfglevel = lvl;
				else
					sm_runlevel(lvl);
				break;

			default:
				dbg("Unsupported runlevel: %d", rq.runlevel);
				break;
			}
			break;

		case INIT_CMD_DEBUG:
			dbg("debug");
			log_debug();
			break;

		case INIT_CMD_RELOAD: /* 'init q' and 'initctl reload' */
			dbg("reload");
			sm_reload();
			break;

		case INIT_CMD_START_SVC:
			dbg("start %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_start(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RESTART_SVC:
			dbg("restart %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_restart(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_STOP_SVC:
			dbg("stop %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_stop(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_RELOAD_SVC:
			dbg("reload %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_reload(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_GET_PLUGINS:
			result = plugin_list(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_PLUGIN_DEPS:
			result = plugin_deps(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_GET_RUNLEVEL:
			dbg("get runlevel");
			rq.runlevel  = runlevel;
			rq.sleeptime = prevlevel;
			break;

		case INIT_CMD_REBOOT:
		case INIT_CMD_HALT:
		case INIT_CMD_POWEROFF:
		case INIT_CMD_SUSPEND:
			result = do_reboot(rq.cmd, rq.sleeptime, rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_ACK:
			dbg("Client failed reading ACK");
			goto leave;

		case INIT_CMD_WDOG_HELLO:
			dbg("wdog hello");
			if (rq.runlevel <= 0) {
				result = 1;
				break;
			}

			dbg("Request to hand-over wdog ... to PID %d", rq.runlevel);
			svc = svc_find_by_pid(rq.runlevel);
			if (!svc) {
				logit(LOG_ERR, "Cannot find PID %d, not registered.", rq.runlevel);
				break;
			}

			if (wdog && wdog != svc) {
				char name[32];

				svc_ident(svc, name, sizeof(name));
				logit(LOG_NOTICE, "Handing over wdog ctrl from %s[%d] to %s[%d]",
				      svc_ident(wdog, NULL, 0), wdog->pid, name, svc->pid);

				if (wdog->protect) {
					logit(LOG_NOTICE, "Stopping and deleting built-in watchdog.");
					stop(wdog, NULL);
					svc_del(wdog);
				}
			}
			wdog = svc;
			break;

		case INIT_CMD_SVC_ITER:
//			dbg("svc iter, first: %d", rq.runlevel);
			/*
			 * XXX: This severely limits the number of
			 * simultaneous client connections, but will
			 * have to do for now.
			 */
			svc = svc_iterator(&iter, rq.runlevel);
			send_svc(sd, svc);
			goto leave;

		case INIT_CMD_SVC_QUERY:
			dbg("svc query: %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_query(rq.data, sizeof(rq.data));
			break;

		case INIT_CMD_SVC_FIND:
			dbg("svc find: %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			send_svc(sd, do_find(rq.data, sizeof(rq.data)));
			goto leave;

		case INIT_CMD_SVC_FIND_BYC:
			dbg("svc find by cond: %s", rq.data);
			strterm(rq.data, sizeof(rq.data));
			send_svc(sd, do_find_byc(rq.data, sizeof(rq.data)));
			goto leave;

		case INIT_CMD_SIGNAL:
			/* runlevel is reused for signal */
			dbg("svc signal %d: %s", rq.runlevel, rq.data);
			strterm(rq.data, sizeof(rq.data));
			result = do_signal(rq.data, sizeof(rq.data), rq.runlevel);
			break;

		default:
			dbg("Unsupported cmd: %d", rq.cmd);
			break;
		}

		if (result)
			rq.cmd = INIT_CMD_NACK;
		else
			rq.cmd = INIT_CMD_ACK;
		len = write(sd, &rq, sizeof(rq));
		if (len != sizeof(rq))
			dbg("Failed sending ACK/NACK back to client");
	}

leave:
	close(sd);
	return;
error:
	api_exit();
	if (api_init(w->ctx))
		errx(1, "Unrecoverable error on API socket");
}

int api_init(uev_ctx_t *ctx)
{
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};
	mode_t oldmask;
	int uid, gid;
	int sd;

	dbg("Setting up external API socket ...");
	sd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (-1 == sd) {
		err(1, "Failed starting external API socket");
		return 1;
	}

	uid = geteuid();
	gid = getgroup(DEFGROUP);

	erase(INIT_SOCKET);
	oldmask = umask(0117);
	if (-1 == bind(sd, (struct sockaddr*)&sun, sizeof(sun)))
		goto error;

	if (-1 == listen(sd, 10))
		goto error;

	if (chown(INIT_SOCKET, uid, gid))
		err(1, "Failed setting group %s on %s", DEFGROUP, INIT_SOCKET);

	umask(oldmask);
	if (!uev_io_init(ctx, &api_watcher, api_cb, NULL, sd, UEV_READ))
		return 0;

error:
	err(1, "Failed initializing API socket");
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
