/* Finit service monitor and heneric API for managing svc_t structures
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
#include <sys/wait.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "sig.h"
#include "svc.h"

/* The registered services are kept in shared memory for easy read-only access
 * by 3rd party APIs.  Mostly because of the ability to, from the outside, query
 * liveness state of all daemons that should run. */
static int svc_counter = 0;		/* Number of registered services to monitor. */
static svc_t *services = NULL;          /* List of registered services, in shared memory. */


/* Connect to/create shared memory area of registered servies */
static void __connect_shm(void)
{
	if (!services) {
		services = finit_svc_connect();
		if (!services) {
			/* This should never happen, but if it does we're probably
			 * knee-deep in more serious problems already... */
			_e("Failed allocating shared memory, error %d: %s", errno, strerror (errno));
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
 * @line:     A complete command line with -- separated description text
 * @username: Optional username to run service as, or %NULL to run as root
 *
 * Actually, the @line can optionally start with a username, denoted by
 * an @ character. Like this:
 *
 *   "service [@username] /path/to/daemon -- Description text"
 *
 * The [] brackets means the argument is optional. If the username is
 * left out the daemon is started as root.
 *
 * Returns:
 * POSIX OK(0) on success, or non-zero errno exit status on failure.
 */
int svc_register(char *line, char *username)
{
	int i = 0;
	char *cmd, *desc;
	svc_t *svc;

	if (!line) {
		_e("Invalid input argument.");
		return errno = EINVAL;
	}

	desc = strstr(line, "-- ");
	if (desc)
		*desc = 0;

	cmd = strtok(line, " ");
	if (!cmd) {
	incomplete:
		_e("Incomplete service, cannot register.");
		return errno = ENOENT;
	}

	if (cmd[0] == '@') {	/* @username */
		username = &cmd[1];

		/* Check if valid command follows... */
		cmd = strtok(NULL, " ");
		if (!cmd)
			goto incomplete;
	}

	svc = svc_new();
	if (!svc) {
		_e("Out of memorym, cannot register service %s", cmd);
		return errno = ENOMEM;
	}

	if (desc)
		strlcpy(svc->desc, desc + 3, sizeof(svc->desc));
	if (username)
		strlcpy(svc->username, username, sizeof(svc->username));
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

void svc_monitor(void)
{
	pid_t lost;
	svc_t *svc;

	lost = waitpid(-1, NULL, WNOHANG);
	if (lost < 1)
		return;

	if (fexist(SYNC_SHUTDOWN) || lost <= 1)
		return;

	/* Power user at the console, don't respawn tasks. */
	if (is_norespawn())
		return;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		char *name = basename(svc->cmd);

		if (lost != svc->pid)
			continue;

		_d("Ouch, lost pid %d - %s(%d)", lost, name, svc->pid);
		if (sig_stopped()) {
			_e("Stopped, not respawning killed processes.");
			break;
		}

		/* Cleanup any lingering or semi-restarting tasks before respawning */
		_d("Sending SIGTERM to service group %s", name);
		procname_kill(name, SIGTERM);

		/* Restarting lost service. */
		if (svc_enabled(svc, 0))
			svc_start(svc);

		break;
	}
}

/* Remember: svc_enabled() must be called before calling svc_start() */
int svc_start(svc_t *svc)
{
	int respawn = svc->pid != 0;
	pid_t pid;
	char *args[MAX_NUM_SVC_ARGS];

	/* Ignore if finit is SIGSTOP'ed */
	if (is_norespawn())
		return 0;

	if (!respawn)
		print_desc("Starting ", svc->desc);

	pid = fork();
	if (pid == 0) {
		int i = 0;
		int uid = getuser(svc->username);
		struct sigaction sa;

		/* Reset signal handlers that were set by the parent process */
                for (i = 1; i < NSIG; i++)
			DFLSIG(sa, i, 0);

		/* Set desired user */
		if (uid >= 0)
			setuid(uid);

		/* Serve copy of args to process in case it modifies them. */
		for (i = 0; svc->args[i][0] != 0 && i < MAX_NUM_SVC_ARGS; i++)
			args[i] = svc->args[i];
		args[i] = NULL;

		if (debug) {
			int fd = open (CONSOLE, O_WRONLY | O_APPEND);
			if (-1 != fd) {
				dup2(STDOUT_FILENO, fd);
				dup2(STDERR_FILENO, fd);
			}

			_e("%starting %s ", respawn ? "Res" : "S", svc->cmd);
			for (i = 0; args[i] && i < MAX_NUM_SVC_ARGS; i++)
				_e("%s ", args[i]);
			_e("");
		}

		execvp(svc->cmd, args);
		exit(0);
	}
	svc->pid = pid;

	if (!respawn)
		print_result(svc->pid > 1 ? 0 : 1);

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

	if (!svc) {
		_e("Failed, no svc pointer.");
		return 1;
	}

	if (svc->pid <= 1) {
		_d("Bad PID %d for %s, SIGTERM", svc->pid, svc->desc);
		svc->pid = 0;
		return 1;
	}

	print_desc("Stopping ", svc->desc);
	_d("Sending SIGTERM to pid:%d name:'%s'", svc->pid, pid_get_name(svc->pid, NULL, 0));
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

	_d("Running svc up hooks ...");
	run_hooks(HOOK_SVC_UP);
}


/**
 * Local Variables:
 *  compile-command: "gcc -g -DUNITTEST -o unittest svc.c"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
