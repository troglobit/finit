/* Generic API for managing svc_t structures
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

#include <string.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "signal.h"
#include "svc.h"

/* The registered services are kept in shared memory for easy read-only access
 * by 3rd party APIs.  Mostly because of the ability to, from the outside, query
 * liveness state of all daemons that should run. */
static int svc_counter = 0;		/* Number of registered services to monitor. */
static svc_t *services = NULL;          /* List of registered services, in shared memory. */
static ev_child watchers[MAX_NUM_SVC];

/* Connect to/create shared memory area of registered servies */
static void __connect_shm(void)
{
	if (!services) {
		services = finit_svc_connect();
		if (!services) {
			/* This should never happen, but if it does we're probably
			 * knee-deep in more serious problems already... */
			_e("Failed setting up shared memory for service monitor.\n");
			abort();
		}
	}
}

/**
 * svc_new - Create a new service object
 *
 * Returns:
 * A pointer to an empty &svc_t object, or %NULL if out of empty slots.
 */
svc_t *svc_new(void)
{
	svc_t *svc = NULL;

	__connect_shm();
	if (svc_counter < MAX_NUM_SVC) {
		svc = &services[svc_counter];
		memset(svc, 0, sizeof(*svc));
		svc->id = svc_counter++;
	} else {
		errno = ENOMEM;
		return NULL;
	}

	return svc;
}

/**
 * svc_find_by_id - Find a service object by its logical ID#
 * @id: Logical ID#
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_id(int id)
{
	__connect_shm();
	if (id >= svc_counter)
		return NULL;

	return &services[id];
}

/**
 * svc_find_by_name - Find a service object by its full path name
 * @name: Full path name, e.g., /sbin/syslogd
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_name(char *name)
{
	int i;

	__connect_shm();
	for (i = 0; i < svc_counter; i++) {
		svc_t *svc = &services[i];

		if (!strncmp(name, svc->cmd, strlen(svc->cmd))) {
			_d("Found a matching svc for %s", name);
			return svc;
		}
	}

	return NULL;
}

/**
 * svc_iterator - Iterates over all registered services.
 * @restart: Get first &svc_t object by setting @restart
 *
 * Returns:
 * The first &svc_t when @restart is set, otherwise the next &svc_t until
 * the end when %NULL is returned.
 */
svc_t *svc_iterator(int restart)
{
	static int id = 0;

	__connect_shm();
	if (restart)
		id = 0;

	if (id >= svc_counter)
		return NULL;

	return &services[id++];
}

/**
 * svc_register - Register a non-backgrounding service to be monitored
 * @line: A complete command line with -- separated description text
 *
 * Returns:
 * POSIX OK(0) on success, or non-zero errno exit status on failure.
 */
int svc_register(char *line)
{
	int i = 0;
	char *cmd, *descr;
	svc_t *svc = svc_new();

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

	return 0;
}


/**
 * svc_enabled - Should the service run?
 * @svc: Pointer to &svc_t object
 * @dynamic: Dynamic event, opaque flag passed to callback
 *
 * This method calls an associated service callback, if registered by a
 * plugin, and returns the &svc_cmd_t status. If no plugin is registered
 * the service is statically enabled in /etc/finit.conf and the result
 * will always be %SVC_START.
 *
 * Returns:
 * Either one of %SVC_START, %SVC_STOP, %SVC_RELOAD.
 */
svc_cmd_t svc_enabled(svc_t *svc, int dynamic)
{
	if (!svc) {
		errno = EINVAL;
		return SVC_STOP;
	}

	if (!svc->plugin) {
		/* Unknown service, default to enabled. */
		return SVC_START;
	}

	return svc->plugin->cb(svc, dynamic);
}

static int is_norespawn(void)
{
	return  sig_stopped()            ||
		fexist("/mnt/norespawn") ||
		fexist("/tmp/norespawn");
}

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

static void svc_monitor(EV_P_ ev_child *w, int UNUSED(revents))
{
	pid_t lost = w->rpid;
	svc_t *svc;

	ev_child_stop(EV_A_ w);

	if (fexist(SYNC_SHUTDOWN) || lost <= 1)
		return;

	/* Power user at the console, don't respawn tasks. */
	if (is_norespawn())
		return;

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
		if (svc_enabled(svc, 0))
			svc_start(svc);

		break;
	}
}

int svc_start(svc_t *svc)
{
	char *args[MAX_NUM_SVC_ARGS];
	int i = 0, restart = 0;
	ev_child *cw = &watchers[svc->id];

	/* Ignore if finit is SIGSTOP'ed */
	if (is_norespawn())
		return 0;

	restart = svc->pid != 0;
	if (!restart) print_descr("Starting ", svc->descr);

	/* Serve copy of args to process in case it modifies them. */
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
	ev_child_init (cw, svc_monitor, svc->pid, 0);
	ev_child_start (EV_DEFAULT_ cw);

	if (!restart) print_result(0);

	return 0;
}

int svc_start_by_name(char *name)
{
	svc_t *svc = svc_find_by_name(name);

	if (svc && svc_enabled(svc, 0))
		return svc_start(svc);

	return 1;
}

int svc_stop(svc_t *svc)
{
	int res = 1;
	ev_child *cw = &watchers[svc->id];

	if (!svc) {
		_e("Failed, no svc pointer.");
		return 1;
	}

	if (svc->pid <= 1) {
		_d("Bad PID %d for %s, SIGTERM", svc->pid, svc->descr);
		svc->pid = 0;
		return 1;
	}

	print_descr("Stopping ", svc->descr);
	_d("Sending SIGTERM to pid:%d name:'%s'", svc->pid, get_pidname(svc->pid, NULL, 0));
	ev_child_stop(ev_default_loop(0), cw);
	res = kill(svc->pid, SIGTERM);
	print_result(res);
	svc->pid = 0;

	return res;
}

int svc_reload(svc_t *svc)
{
	if (!svc) {
		_e("Failed, no svc pointer.");
		return 1;
	}

	if (svc->pid <= 1) {
		_d("Bad PID %d for %s, SIGHUP", svc->pid, svc->cmd);
		svc->pid = 0;
		return 1;
	}

	_d("Sending SIGHUP to PID %d", svc->pid);
	return kill(svc->pid, SIGHUP);
}

void svc_start_all(void)
{
	svc_t *svc;
	svc_cmd_t cmd;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		cmd = svc_enabled(svc, 0);
		if (cmd == SVC_START || (cmd == SVC_RELOAD && svc->pid == 0))
			svc_start(svc);
		else if (cmd == SVC_RELOAD)
			svc_reload(svc);
	}

	/* XXX: Hook point for plugins. */
}


/**
 * Local Variables:
 *  compile-command: "gcc -g -DUNITTEST -o unittest svc.c"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
