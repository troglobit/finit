/* Service starter and supervisor, inspired by daemontools
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
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

#include <features.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>		/* mknod() */
#include <sys/wait.h>		/* wait() */

#include "finit.h"
#include "ipc.h"
#include "svc.h"
#include "service.h"
#include "helpers.h"

static int dynamic_event_arg = 0;	/* XXX: Fragile construct, refactor! --Jocke */
static int sd = -1;

/* Returns the event type unless it's a reconfiguration event. */
static int is_dynamic_event(generic_event_t *event)
{
	if (!event) {
		_e("Invalid event pointer received.");
		return 0;
	}

	/* XXX: Hook point for plugins. */

	/* XXX: Fragile construct, refactor! --Jocke */
	dynamic_event_arg = (int)event->arg;

	return (int)event->mtype;
}

/* Return value:
 *   0 - Disabled
 *   1 - Enabled
 *   2 - Enabled, needs restart
 */
int service_enabled(svc_t *svc, int dynamic)
{
	int run;

	if (!svc->reg.cb) {
		/* Unknown service, default to enabled. */
		return 1;
	}

	run = svc->reg.cb(svc, dynamic);
	/* Sanity check callback return value */
	if (run < 0 || run > 2) {
		_d("Callback for %s returned invalid value %d, disabling svc.", svc->reg.path, run);
		run = 0;
	}

	return run;
}


/* This is a map of all known services that we care for.  All others, found in
 * the system finit.conf file, use the default handler. */
static svc_map_t known_services[] = {
	{ NULL, NULL, 0, 0, 0},
};

/* Used to cleanup any lingering or semi-restarting tasks before respawning. */
static int kill_service(svc_t *svc, int signo)
{
	char *name = basename(svc->cmd);

	_d("Sending signal %d to all processes named %s", signo, name);
	kill_procname(name, signo);

	/* Don't restart immediately, some throttling necessary.
	 * Also it's good form to let the application terminate properly. */
	sleep(1);

	return 0;
}

/**
 * service_register - Register a non-backgrounding service to be monitored
 * @line: A complete command line with -- separated description text
 *
 * Returns:
 * POSIX OK(0) on success, or non-zero errno exit status on failure.
 */
int service_register(char *line)
{
	int i = 0;
	char *cmd, *descr;
	svc_t *svc = svc_new_service();

	if (!svc || !line) {
		return -EINVAL;
	}

	descr = strstr(line, "-- ");
	if (descr) {
		*descr = 0;
		strlcpy(svc->descr, descr + 3, sizeof(svc->descr));
	}

	cmd = strtok(line, " ");
	strlcpy(svc->cmd, cmd, sizeof(svc->cmd));
	strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));

	while ((cmd = strtok(NULL, " ")))
		strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));

	svc->args[i][0] = 0;

	/* If this is a known service, then register callbacks and other data. */
	for (i = 0; known_services[i].path; i++) {
		svc_map_t *curr = &known_services[i];

		if (!strncmp(curr->path, svc->cmd, strlen(svc->cmd))) {
			memcpy(&svc->reg, curr, sizeof(svc->reg));
		}
	}

	return 0;
}

#if 0 /* May be usefull someday. */
static int poll_for_pidfile(char *cmd, const char *path)
{
	int pid = 0;
	int tries = 0;

	/* Timeout = 100 * 50ms = 5s */
	while (-1 == access(path, F_OK) && tries++ < 100) {
		/* Wait 50ms between retries */
		usleep(50000);
	}

	if (-1 == access(path, F_OK)) {
		_e("Timeout! No PID found for %s, pidfile %s does not exist?", cmd, path);
		pid = 0;
	} else {
		pid = pidfile_read_pid(path);
	}

	return pid;
}
#endif
/* See the README in this package for details on how services are started, stopped
 * and how the whole process ties together with the rest of the system.
 *			    --Jocke 2009-03-18 */
/* Remember: service_enabled() must be called before calling service_start() */
int service_start(svc_t *svc)
{
	char *args[MAX_NUM_SVC_ARGS];
	int i = 0, restart = 0;

	/* Ignore if finit is SIGSTOP'ed */
	if (sig_stopped())
		return 0;

	restart = svc->pid != 0;
	if (!restart)
		print_descr("Starting ", svc->descr);
	while (svc->args[i][0] != 0 && i < MAX_NUM_SVC_ARGS) {
		args[i] = svc->args[i];
		i++;
	}
	args[i] = NULL;

	if (debug) {
		_e("(re)starting %s ", svc->cmd);
		for (i = 0; args[i] && i < MAX_NUM_SVC_ARGS; i++)
			_e("%s ", args[i]);
		_e("");
	}
	svc->pid = start_process(svc->cmd, args, debug);

	if (!restart)
		print_result(0);

	return 0;
}

/* Tries to find the matching service and start it. */
int service_start_by_name(char *name)
{
	svc_t *svc = svc_find_by_name (name);

	if (svc && service_enabled(svc, 0)) {
		return service_start(svc);
	}

	return 1;
}

/* See the README in this package for details on how services are started, reloaded
 * (config), stopped and how the whole process ties together with the rest of the
 * system.  --Jocke 2009-03-18 */
static int stop_service(svc_t *svc)
{
	int res = 1;

	if (!svc) {
		_e("Failed, no svc pointer.");
		return 1;
	}

	if (svc->pid <= 1) {
		_d("bad pid:%d for %s, SIGTERM", svc->pid, svc->descr);
		svc->pid = 0;
		return 1;
	}

	print_descr("Stopping ", svc->descr);
	_d("Sending SIGTERM to pid:%d name:'%s'", svc->pid, get_pidname(svc->pid, NULL, 0));
	res = kill(svc->pid, SIGTERM);
	print_result(res);
	svc->pid = 0;

	return res;
}

/* See the README in this package for details on how services are started, reloaded
 * (config), stopped and how the whole process ties together with the rest of the
 * system.  --Jocke 2009-03-18 */
static int reload_service(svc_t *svc)
{
	int res;

	if (!svc) {
		_e("Failed, no svc pointer.");
		return 1;
	}

	if (svc->pid <= 1) {
		_d("Bad PID %d for %s", svc->pid, svc->cmd);
		svc->pid = 0;
		return 1;
	}

	_d("Sending SIGHUP to PID %d", svc->pid);
	res = kill(svc->pid, SIGHUP);

	return res;
}

static int wait_for_event(generic_event_t *event, int timeout)
{
	if (-1 == sd) {
		sd = ipc_init(SINGLE_TIPC_NODE, FINIT_MESSAGE_BUS, SERVER_CONNECTION);
		if (-1 == sd) {
			return 1;
		}
	}

	/* ipc_receive_event() might be interrupted by signal, errno is then set to EINTR */
	return ipc_receive_event(sd, event, sizeof(*event), timeout);
}

/* Returns 1 if pid is alive (/proc/pid exists) else 0 */
static int pid_alive(unsigned int pid)
{
	char name[24]; /* Enough for max pid_t */

	snprintf(name, sizeof(name), "/proc/%d", pid);
	return !access(name, F_OK);
}

void service_startup(void)
{
	int ret;
	svc_t *svc;

	/* Start finit listen socket before start any services, this to
	 * be sure we don't miss any messages from for example linkd */
	if (-1 == sd) {
		sd = ipc_init(SINGLE_TIPC_NODE, FINIT_MESSAGE_BUS, SERVER_CONNECTION);
		if (-1 == sd) {
			return;
		}
	}


	/* Start 'em */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		/* Start, when pid==0 we start regardless of ret. */
		ret = service_enabled(svc, 0);
		if (ret == 1 || (ret && svc->pid == 0))
			service_start(svc);
		else if (ret == 2)
			reload_service(svc);
	}

	/* XXX: Hook point for plugins. */
}

static int is_norespawn(void)
{
	return  sig_stopped()            ||
		fexist("/mnt/norespawn") ||
		fexist("/tmp/norespawn");
}

/* See the README in this package for details on how services are started, stopped
 * and how the whole process ties together with the rest of the system.
 *			    --Jocke 2009-03-18 */
void reiterate(generic_event_t *event)
{
	int ret;
	int any_stopped = 0;
	svc_t *svc;

	_d("event 0x%x", (unsigned int)event->mtype);
	/* Reset state variables before each new run. */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		svc->reload  = 0;
		svc->private = 0;
	}

	/* Stop services */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		/* Only do configuration changes here, e.g. stopping disabled services, etc. */
		if (is_dynamic_event(event) && !svc->reg.dyn_workaround) // XXX: Ugly workaround
			continue;

		svc->reload = 0;
		if (svc->pid) {
			ret = service_enabled(svc, svc->reg.dyn_workaround ? is_dynamic_event(event) : 0);
			if (2 == ret) {
				_d("Scheduling %s[%d] for delayed reload.", svc->cmd, svc->pid);
				svc->reload = 1;
			} else if (0 == ret) {
				_d("Attempting to stop %s[%d]", svc->cmd, svc->pid);
				if (svc->pid && !stop_service(svc)) {
					any_stopped++;
				}
			}
		}
	}

	/* Wait for any stopped processes before notifying hwsetup. */
	if (any_stopped)
		sleep(1);

	/* XXX: Hook point for plugins. */

	/* (Re)Start services */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		/* Filter out services not interested in dynamic changes. */
		if (is_dynamic_event(event) && !(svc->reg.dyn_reload || svc->reg.vlans_changed_reload || svc->reg.dyn_workaround))
			continue;

		/* Check for stale PID!  Happens if, e.g., SIGCHLD signal is lost */
		if (svc->pid && !pid_alive(svc->pid)) {
			_d("PID:%d (%s) does not seem to be running, scheduling restart.", svc->pid, svc->cmd);
			svc->pid = 0;
			svc->reload = 0;
			goto restart;
		}

		if (svc->reload) {
			svc->reload = 0;
			reload_service(svc);
		} else {
			int run;
		restart:
			run = service_enabled(svc, is_dynamic_event(event));
			if (run) {
				if (!svc->pid && !is_norespawn()) {
					service_start(svc);
				} else if (2 == run) {
					reload_service(svc);
				}
			}
		}
	}

	/* Notify configuration frontends that we're done. */
	if (!is_dynamic_event(event)) {
		/* XXX: Hook point for plugins. */
	}
}

void service_monitor(void)
{
	int status;
	pid_t lost;
	svc_t *svc;
	generic_event_t event;

	while (1) {
		/* Poll, timeout 1000 ms, for message from the system, or SIGCHLD
		 * when children dies. */
		if (!wait_for_event(&event, 1000)) {
			_d("Received event 0x%lx", event.mtype);
			/* XXX: Hook point for plugins. */
		}

		lost = waitpid(-1, &status, WNOHANG);
		if (fexist(SYNC_SHUTDOWN) || lost <= 1)
			continue;

		/* Power user at the console, don't respawn tasks. */
		if (is_norespawn())
			return;

		/* Monitor 'em */
		for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
			if (lost != svc->pid)
				continue;

			if (sig_stopped()) {
				_e("Stopped, not respawning killed processes.");
				break;
			}

			_d("Ouch, lost pid %d - %s(%d)", lost, svc->cmd, svc->pid);
			kill_service(svc, SIGTERM);

			/* Restarting lost service. */
			if (service_enabled(svc, 0))
				service_start(svc);
		}
	}
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
