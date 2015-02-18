/* Finit service monitor, task starter and generic API for managing svc_t
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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
#include <utmp.h>
#include <netdb.h>
#include <net/if.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "sig.h"
#include "tty.h"
#include "lite.h"
#include "svc.h"
#include "inetd.h"

/* The registered services are kept in shared memory for easy read-only access
 * by 3rd party APIs.  Mostly because of the ability to, from the outside, query
 * liveness state of all daemons that should run. */
static int was_stopped = 0;
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
		svc = &services[svc_counter++];
		memset(svc, 0, sizeof(*svc));
	} else {
		errno = ENOMEM;
		return NULL;
	}

	return svc;
}

/**
 * svc_find - Find a service object by its full path name
 * @name: Full path name, e.g., /sbin/syslogd
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find(char *name)
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
	static int i = 0;

	__connect_shm();
	if (restart)
		i = 0;

	if (i >= svc_counter)
		return NULL;

	return &services[i++];
}

/**
 * svc_bootstrap - Start bootstrap services and tasks
 *
 * System startup, runlevel S, where only services, tasks and
 * run commands absolutely essential to bootstrap are located.
 */
void svc_bootstrap(void)
{
	svc_t *svc;

	_d("Bootstrapping all services in runlevel S from %s", FINIT_CONF);
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		svc_cmd_t cmd;

		if (svc->type == SVC_CMD_INETD)
			continue;

		cmd = svc_enabled(svc, 0, NULL);
		if (SVC_START == cmd  || (SVC_RELOAD == cmd))
			svc_start(svc);
	}
}

static int encode(int lvl)
{
	if (!lvl)
		return 0;

	return lvl + '0';
}

static void utmp_save(int pre, int now)
{
	struct utmp utent;

	utent.ut_type  = RUN_LVL;
	utent.ut_pid   = (encode(pre) << 8) | (encode(now) & 0xFF);
	strlcpy(utent.ut_user, "runlevel", sizeof(utent.ut_user));

	setutent();
	pututline(&utent);
	endutent();
}

/**
 * svc_runlevel - Change to a new runlevel
 * @newlevel: New runlevel to activate
 *
 * Stops all services not in @newlevel and starts, or lets continue to run,
 * those in @newlevel.  Also updates @prevlevel and active @runlevel.
 */
void svc_runlevel(int newlevel)
{
	svc_t *svc;

	if (runlevel == newlevel)
		return;

	if (newlevel < 0 || newlevel > 9)
		return;

	prevlevel = runlevel;
	runlevel  = newlevel;
	utmp_save(prevlevel, newlevel);

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		svc_cmd_t cmd;

		if (svc->type == SVC_CMD_INETD)
			continue;

		cmd = svc_enabled(svc, 0, NULL);
		if (svc->pid) {
			if (cmd == SVC_STOP)
				svc_stop(svc);
			else if (cmd == SVC_RELOAD)
				svc_reload(svc);
		} else {
			if (cmd == SVC_START)
				svc_start(svc);
		}
	}

	if (0 == runlevel) {
		do_shutdown(SIGUSR2);
		return;
	}
	if (6 == runlevel) {
		do_shutdown(SIGUSR1);
		return;
	}

	if (runlevel == 1)
		touch("/etc/nologin");	/* Disable login in single-user mode */
	else
		erase("/etc/nologin");

	if (0 != prevlevel)
		tty_runlevel(runlevel);
}

/**
 * svc_register - Register service, task or run commands
 * @type:     %SVC_CMD_SERVICE(0), %SVC_CMD_TASK(1), %SVC_CMD_RUN(2)
 * @line:     A complete command line with -- separated description text
 * @username: Optional username to run service as, or %NULL to run as root
 *
 * This function is used to register commands to be run on different
 * system runlevels with optional username.  The @type argument details
 * if it's service to bo monitored/respawned (daemon), a one-shot task
 * or a command that must run in sequence and not in parallell, like
 * service and task commands do.
 *
 * The @line can optionally start with a username, denoted by an @
 * character. Like this:
 *
 *   "service @username [!0-6,S] /path/to/daemon arg -- Description text"
 *   "task @username [!0-6,S] /path/to/task arg -- Description text"
 *   "run  @username [!0-6,S] /path/to/cmd arg -- Description text"
 *   "inetd tcp/ssh nowait [2345] @root:root /usr/sbin/sshd -i -- Description"
 *
 * If the username is left out the command is started as root.
 * Inetd services, launched on demand (SVC_CMD_INETD)
 * The [] brackets are there to denote the allowed runlevels. Allowed
 * runlevels mimic that of SysV init with the addition of the 'S'
 * runlevel, which is only run once at startup. It can be seen as the
 * system bootstrap. If a task or run command is listed in more than the
 * [S] runlevel they will be called when changing runlevel.
 *
 * Returns:
 * POSIX OK(0) on success, or non-zero errno exit status on failure.
 */
int svc_register(int type, char *line, char *username)
{
	int i = 0, forking = 0;
	char *service = NULL, *proto = NULL;
	char *cmd, *desc, *runlevels = NULL;
	svc_t *svc;
	plugin_t *plugin = NULL;
	struct servent *sv = NULL;
	struct protoent *pv = NULL;

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

	while (cmd) {
		if (cmd[0] != '/' && strchr(cmd, '/'))
			service = cmd;   /* inetd port/proto */
		else if (!strncasecmp(cmd, "nowait", 6))
			forking = 1;
		else if (!strncasecmp(cmd, "wait", 4))
			forking = 0;
		else if (cmd[0] == '@')	/* @username[:group] */
			username = &cmd[1];
		else if (cmd[0] == '[')	/* [runlevels] */
			runlevels = &cmd[0];
		else
			break;

		/* Check if valid command follows... */
		cmd = strtok(NULL, " ");
		if (!cmd)
			goto incomplete;
	}

	if (service) {
		proto = strchr(service, '/');
		*proto++ = 0;
		sv = getservbyname(service, proto);
		if (!sv) {
			_pe("Invalid inetd %s/%s (service/proto), skipping", service, proto);
			return errno = EINVAL;
		}

		pv = getprotobyname(sv->s_proto);
		if (!pv) {
			_pe("Cannot find proto %s, skipping.", sv->s_proto);
			return errno = EINVAL;
		}

		_d("Got service %s:%d proto %s:%d", service, ntohs(sv->s_port), sv->s_proto, pv->p_proto);
	}

	/* Find plugin that provides a callback for this inetd service */
	if (type == SVC_CMD_INETD && !strncasecmp(cmd, "internal", 8)) {
		plugin = plugin_find(service);
		if (!plugin || !plugin->inetd.cmd) {
			_e("Inetd service %s has no internal plugin, skipping.", service);
			return errno = ENOENT;
		}
	}

	svc = svc_new();
	if (!svc) {
		_e("Out of memory, cannot register service %s", cmd);
		return errno = ENOMEM;
	}

	svc->type = type;

	if (desc)
		strlcpy(svc->desc, desc + 3, sizeof(svc->desc));

	if (username) {
		char *ptr = strchr(username, ':');

		if (ptr) {
			*ptr++ = 0;
			strlcpy(svc->group, ptr, sizeof(svc->group));
		}
		strlcpy(svc->username, username, sizeof(svc->username));
	}

	if (sv) {
		strlcpy(svc->service, service, sizeof(svc->service));
		svc->port  = ntohs(sv->s_port);
		svc->proto = pv->p_proto;

		/* Naïve mapping tcp->stream, udp->dgram, other->dgram */
		if (!strcasecmp(sv->s_proto, "tcp"))
			svc->sock_type = SOCK_STREAM;
		else
			svc->sock_type = SOCK_DGRAM;

		svc->forking = forking;
	}

	if (plugin) {
		/* Internal plugin provides this service */
		svc->internal_cmd = plugin->inetd.cmd;
	} else {
		/* External program to call */
		strlcpy(svc->cmd, cmd, sizeof(svc->cmd));

		strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));
		while ((cmd = strtok(NULL, " ")))
			strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));
		svc->args[i][0] = 0;
	}

	svc->runlevels = parse_runlevels(runlevels);
	_d("Service %s runlevel 0x%2x", svc->cmd, svc->runlevels);

	return 0;
}


/**
 * svc_enabled - Should the service run?
 * @svc:   Pointer to &svc_t object
 * @event: Dynamic event, opaque flag passed to callback
 * @arg:   Event argument, used only by external service plugins.
 *
 * This method calls an associated service callback, if registered by a
 * plugin, and returns the &svc_cmd_t status. If no plugin is registered
 * the service is statically enabled in /etc/finit.conf and the result
 * will always be %SVC_START.
 *
 * Returns:
 * Either one of %SVC_START, %SVC_STOP, %SVC_RELOAD.
 */
svc_cmd_t svc_enabled(svc_t *svc, int event, void *arg)
{
	if (!svc) {
		errno = EINVAL;
		return SVC_STOP;
	}

	if (!ISSET(svc->runlevels, runlevel))
		return SVC_STOP;

	/* Is there a service plugin registered? */
	if (svc->cb) {
		int   status;
		pid_t pid;

		/* Let callback run in separate process so it doesn't crash PID 1 */
		pid = fork();
		if (-1 == pid) {
			_pe("Failed in %s callback", svc->cmd);
			return SVC_STOP;
		}

		if (!pid) {
			status = svc->cb(svc, event, arg);
			exit(status);
		}

		if (waitpid(pid, &status, 0) == -1) {
			_pe("Failed reading status from %s callback", svc->cmd);
			return SVC_STOP;
		}

		/* Callback normally exits here. */
		if (WIFEXITED(status))
			return WEXITSTATUS(status);

		/* Check for SEGFAULT or other error ... */
		if (WCOREDUMP(status))
			_e("Callback to %s crashed!\n", svc->cmd);
		else
			_e("Callback to %s did not exit normally!\n", svc->cmd);

		return SVC_STOP;
	}

	/* No service plugin, default to start, since listed in finit.conf */
	return SVC_START;
}

static int is_norespawn(void)
{
	return  sig_stopped()            ||
		fexist("/mnt/norespawn") ||
		fexist("/tmp/norespawn");
}

static void restart_any_lost_procs(void)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->type == SVC_CMD_INETD)
			continue;

		if (svc->pid > 0 && pid_alive(svc->pid))
			continue;

		svc_start(svc);
	}
}

void svc_monitor(pid_t lost)
{
	svc_t *svc;

	if (was_stopped && !is_norespawn()) {
		was_stopped = 0;
		restart_any_lost_procs();
		return;
	}

	if (fexist(SYNC_SHUTDOWN) || lost <= 1)
		return;

	/* Power user at the console, don't respawn tasks. */
	if (is_norespawn()) {
		was_stopped = 1;
		return;
	}

	if (tty_respawn(lost))
		return;

	if (inetd_respawn(lost))
		return;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (lost != svc->pid)
			continue;

		if (SVC_CMD_SERVICE != svc->type) {
			svc->pid = 0;
			continue;
		}

		_d("Ouch, lost pid %d - %s(%d)", lost, basename(svc->cmd), svc->pid);

		/* No longer running, update books. */
		svc->pid = 0;

		if (sig_stopped()) {
			_e("Stopped, not respawning killed processes.");
			break;
		}

		/* Cleanup any lingering or semi-restarting tasks before respawning */
		_d("Sending SIGTERM to service group %s", basename(svc->cmd));
		procname_kill(basename(svc->cmd), SIGTERM);

		/* Restarting lost service. */
		if (svc_enabled(svc, 0, NULL)) {
			svc->restart_counter++;
			svc_start(svc);
		}

		break;
	}
}

/* Remember: svc_enabled() must be called before calling svc_start() */
int svc_start(svc_t *svc)
{
	int respawn, sd = 0;
	char ifname[IF_NAMESIZE] = "UNKNOWN";
	pid_t pid;
	sigset_t nmask, omask;

	if (!svc)
		return 0;
	respawn = svc->pid != 0;

	/* Don't try and start service if it doesn't exist. */
	if (!fexist(svc->cmd) && !svc->internal_cmd) {
		char msg[80];

		snprintf(msg, sizeof(msg), "Service %s does not exist!", svc->cmd);
		print_desc("", msg);
		print_result(1);

		return 0;
	}

	/* Ignore if finit is SIGSTOP'ed */
	if (is_norespawn())
		return 0;

	if (SVC_CMD_INETD == svc->type) {
		sd = svc->watcher.fd;

		if (svc->sock_type == SOCK_STREAM) {
			/* Open new client socket from server socket */
			sd = accept(sd, NULL, NULL);
			if (sd < 0) {
				FLOG_PERROR("Failed accepting inetd service %d/tcp", svc->port);
				return 0;
			}

			_d("New client socket %d accepted for inetd service %d/tcp", sd, svc->port);

			/* Find ifname by means of getsockname() and getifaddrs() */
			inetd_stream_peek(sd, ifname);
		} else {           /* SOCK_DGRAM */
			/* Find ifname by means of IP_PKTINFO sockopt --> ifindex + if_indextoname() */
			inetd_dgram_peek(sd, ifname);
		}

		/* XXX: Add poor man's tcpwrappers here, we now know inbound ifname ... */

		FLOG_INFO("Starting inetd service %s for requst from iface %s ...", svc->service, ifname);
	}
	else if (SVC_CMD_SERVICE != svc->type)
		print_desc("", svc->desc);
	else if (!respawn)
		print_desc("Starting ", svc->desc);

	/* Block sigchild while forking.  */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);

	pid = fork();
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (pid == 0) {
		int i = 0;
		int status;
		int uid = getuser(svc->username);
		struct sigaction sa;
		char *args[MAX_NUM_SVC_ARGS];

		sigemptyset(&nmask);
		sigaddset(&nmask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask, NULL);

		/* Reset signal handlers that were set by the parent process */
		for (i = 1; i < NSIG; i++)
			DFLSIG(sa, i, 0);

		/* Set desired user */
		if (uid >= 0)
			setuid(uid);

		/* Serve copy of args to process in case it modifies them. */
		for (i = 0; i < (MAX_NUM_SVC_ARGS - 1) && svc->args[i][0] != 0; i++)
			args[i] = svc->args[i];
		args[i] = NULL;

		/* Redirect inetd socket to stdin for service */
		if (SVC_CMD_INETD == svc->type) {
			/* sd set previously */
			dup2(sd, STDIN_FILENO);
			close(sd);
			dup2(STDIN_FILENO, STDOUT_FILENO);
			dup2(STDIN_FILENO, STDERR_FILENO);
		} else if (debug) {
			int fd;
			char buf[CMD_SIZE] = "";

			fd = open(CONSOLE, O_WRONLY | O_APPEND);
			if (-1 != fd) {
				dup2(fd, STDOUT_FILENO);
				dup2(fd, STDERR_FILENO);
				close(fd);
			}

			for (i = 0; i < MAX_NUM_SVC_ARGS && args[i]; i++) {
				char arg[MAX_ARG_LEN];

				snprintf(arg, sizeof(arg), "%s ", args[i]);
				if (strlen(arg) < (sizeof(buf) - strlen(buf)))
					strcat(buf, arg);
			}
			_e("%starting %s: %s", respawn ? "Res" : "S", svc->cmd, buf);
		}

		if (svc->internal_cmd)
			status = svc->internal_cmd(svc->sock_type);
		else
			status = execv(svc->cmd, args); /* XXX: Maybe use execve() to be able to launch scripts? */

		exit(status);
	}
	svc->pid = pid;

	if (SVC_CMD_INETD == svc->type) {
		if (svc->sock_type == SOCK_STREAM)
			close(sd);
	}
	else if (SVC_CMD_RUN == svc->type)
		print_result(WEXITSTATUS(complete(svc->cmd, pid)));
	else if (!respawn)
		print_result(svc->pid > 1 ? 0 : 1);

	return 0;
}

int svc_start_by_name(char *name)
{
	svc_t *svc = svc_find(name);

	if (svc && svc_enabled(svc, 0, NULL))
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
		svc->restart_counter = 0;
		return 1;
	}

	if (SVC_CMD_SERVICE != svc->type)
		return 0;

	if (runlevel != 1)
		print_desc("Stopping ", svc->desc);
	_d("Sending SIGTERM to pid:%d name:'%s'", svc->pid, pid_get_name(svc->pid, NULL, 0));
	res = kill(svc->pid, SIGTERM);
	if (runlevel != 1)
		print_result(res);
	svc->pid = 0;
	svc->restart_counter = 0;

	return res;
}

int svc_reload(svc_t *svc)
{
	/* Ignore if finit is SIGSTOP'ed */
	if (is_norespawn())
		return 0;

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

/**
 * Local Variables:
 *  compile-command: "gcc -g -DUNITTEST -o unittest svc.c"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
