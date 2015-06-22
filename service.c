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
#include <net/if.h>
#include "libite/lite.h"

#include "finit.h"
#include "conf.h"
#include "helpers.h"
#include "private.h"
#include "sig.h"
#include "tty.h"
#include "service.h"
#include "inetd.h"

#define RESPAWN_MAX    10	        /* Prevent endless respawn of faulty services. */

static int    is_norespawn       (void);
static void   restart_lost_procs (void);
static void   svc_dance          (svc_t *svc);
static svc_t *find_inetd_svc     (char *path, char *service, char *proto);

	
/**
 * service_bootstrap - Start bootstrap services and tasks
 *
 * System startup, runlevel S, where only services, tasks and
 * run commands absolutely essential to bootstrap are located.
 */
void service_bootstrap(void)
{
	svc_t *svc;

	_d("Bootstrapping all services in runlevel S from %s", FINIT_CONF);
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		svc_cmd_t cmd;

		/* Inetd services cannot be part of bootstrap currently. */
		if (svc_is_inetd(svc))
			continue;

		cmd = service_enabled(svc, 0, NULL);
		if (SVC_START == cmd  || (SVC_RELOAD == cmd))
			service_start(svc);
	}
}

/**
 * service_enabled - Should the service run?
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
svc_cmd_t service_enabled(svc_t *svc, int event, void *arg)
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

/* Remember: service_enabled() must be called before calling service_start() */
int service_start(svc_t *svc)
{
	int respawn, sd = 0;
	pid_t pid;
	sigset_t nmask, omask;

	if (!svc)
		return 1;
	respawn = svc->pid != 0;

	/* Don't try and start service if it doesn't exist. */
	if (!fexist(svc->cmd) && !svc->inetd.cmd) {
		if (verbose) {
			char msg[80];

			snprintf(msg, sizeof(msg), "Service %s does not exist!", svc->cmd);
			print_desc("", msg);
			print_result(1);
		}

		return 1;
	}

	/* Ignore if finit is SIGSTOP'ed */
	if (is_norespawn())
		return 0;

#ifndef INETD_DISABLED
	if (svc_is_inetd(svc)) {
		char ifname[IF_NAMESIZE] = "UNKNOWN";

		sd = svc->inetd.watcher.fd;

		if (svc->inetd.type == SOCK_STREAM) {
			/* Open new client socket from server socket */
			sd = accept(sd, NULL, NULL);
			if (sd < 0) {
				FLOG_PERROR("Failed accepting inetd service %d/tcp", svc->inetd.port);
				return 1;
			}

			_d("New client socket %d accepted for inetd service %d/tcp", sd, svc->inetd.port);

			/* Find ifname by means of getsockname() and getifaddrs() */
			inetd_stream_peek(sd, ifname);
		} else {           /* SOCK_DGRAM */
			/* Find ifname by means of IP_PKTINFO sockopt --> ifindex + if_indextoname() */
			inetd_dgram_peek(sd, ifname);
		}

		if (!inetd_is_allowed(&svc->inetd, ifname)) {
			FLOG_INFO("Service %s on port %d not allowed from interface %s.",
				  svc->inetd.name, svc->inetd.port, ifname);
			if (svc->inetd.type == SOCK_STREAM)
				close(sd);

			return 1;
		}

		FLOG_INFO("Starting inetd service %s for requst from iface %s ...", svc->inetd.name, ifname);
	} else
#endif
	if (verbose) {
		if (svc_is_daemon(svc))
			print_desc("", svc->desc);
		else if (!respawn)
			print_desc("Starting ", svc->desc);
	}

	/* Block sigchild while forking.  */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);

	pid = fork();
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (pid == 0) {
		int i = 0;
		int status;
#ifdef ENABLE_STATIC
		int uid = 0; /* XXX: Fix better warning that dropprivs is disabled. */
#else
		int uid = getuser(svc->username);
#endif
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
		if (svc_is_inetd(svc)) {
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

		if (svc->inetd.cmd)
			status = svc->inetd.cmd(svc->inetd.type);
		else
			status = execv(svc->cmd, args); /* XXX: Maybe use execve() to be able to launch scripts? */

		if (svc_is_inetd(svc)) {
			if (svc->inetd.type == SOCK_STREAM) {
				close(STDIN_FILENO);
				close(STDOUT_FILENO);
				close(STDERR_FILENO);
			}
		}

		exit(status);
	}
	svc->pid = pid;

	if (svc_is_inetd(svc)) {
		if (svc->inetd.type == SOCK_STREAM)
			close(sd);
	} else {
		int result;

		if (SVC_TYPE_RUN == svc->type)
			result = WEXITSTATUS(complete(svc->cmd, pid));
		else if (!respawn)
			result = svc->pid > 1 ? 0 : 1;
		else
			result = 0;

		if (verbose)
			print_result(result);
	}

	return 0;
}

int service_stop(svc_t *svc)
{
	int res = 1;

	if (!svc)
		return 1;

	if (svc->pid <= 1) {
		_d("Bad PID %d for %s, SIGTERM", svc->pid, svc->desc);
		res = 1;
		goto exit;
	}

	if (SVC_TYPE_SERVICE != svc->type)
		return 0;

	if (runlevel != 1 && verbose)
		print_desc("Stopping ", svc->desc);

	_d("Sending SIGTERM to pid:%d name:%s", svc->pid, pid_get_name(svc->pid, NULL, 0));
	res = kill(svc->pid, SIGTERM);

	if (runlevel != 1 && verbose)
		print_result(res);
exit:
	svc->pid = 0;
	svc->restart_counter = 0;

	return res;
}

/* stop + start */
int service_restart(svc_t *svc)
{
	int result = 0;

	if (!svc)
		return 1;

	result += service_stop(svc);
	sleep(1);
	result += service_start(svc);

	return result;
}

/**
 * service_reload - Send SIGHUP to a service
 * @svc: Service to reload
 *
 * This function does some basic checks of the runtime state of Finit
 * and a sanity check of the @svc before sending %SIGHUP.
 * 
 * Returns:
 * POSIX OK(0) or non-zero on error.
 */
int service_reload(svc_t *svc)
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
 * service_reload_dynamic - Reload modifued dynamic services
 *
 * This function is called when Finit has recieved SIGHUP to reload
 * .conf files in /etc/finit.d.  It is responsible for starting,
 * stopping and reloading (forwarding SIGHUP) to processes affected.
 */
void service_reload_dynamic(void)
{
	int num = 0;
	svc_t *svc;

	_d("Stopping disabled/removed services ...");
	for (svc = svc_dynamic_iterator(1); svc; svc = svc_dynamic_iterator(0)) {
		if (svc->dirty != 0) {
			service_stop(svc);
			num++;
		}
	}

	_d("All disabled/removed services have been stoppped, calling reconf hooks ...");
	plugin_run_hooks(HOOK_SVC_RECONF); /* Reconfigure HW/VLANs/etc here */

	_d("Starting enabled/added services ...");
	for (svc = svc_dynamic_iterator(1); svc; svc = svc_dynamic_iterator(0)) {
		if (svc->dirty > 0)
			svc_dance(svc);
	}
}

/**
 * service_runlevel - Change to a new runlevel
 * @newlevel: New runlevel to activate
 *
 * Stops all services not in @newlevel and starts, or lets continue to run,
 * those in @newlevel.  Also updates @prevlevel and active @runlevel.
 */
void service_runlevel(int newlevel)
{
	svc_t *svc;

	if (runlevel == newlevel)
		return;

	if (newlevel < 0 || newlevel > 9)
		return;

	prevlevel = runlevel;
	runlevel  = newlevel;

	_d("Setting new runlevel --> %d <-- previous %d", runlevel, prevlevel);
	runlevel_set(prevlevel, newlevel);

	/* Make sure to reload all *.conf in /etc/finit.d/ */
	svc_mark_dynamic();
	parse_finit_d(rcsd);

	_d("Stopping services services not allowed in new runlevel ...");
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!ISSET(svc->runlevels, runlevel)) {
#ifndef INETD_DISABLED
			if (svc_is_inetd(svc))
				inetd_stop(&svc->inetd);
			else
#endif
				service_stop(svc);
		}

		/* ... or disabled/removed services from /etc/finit.d/ */
		if (svc->mtime && svc->dirty != 0)
			service_stop(svc);
	}

	/* Prev runlevel services stopped, call hooks before starting new runlevel ... */
	_d("All services have been stoppped, calling runlevel change hooks ...");
	plugin_run_hooks(HOOK_RUNLEVEL_CHANGE);  /* Reconfigure HW/VLANs/etc here */

	_d("Starting services services new to this runlevel ...");
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
#ifndef INETD_DISABLED
		/* Inetd services have slightly different semantics */
		if (svc_is_inetd(svc)) {
			if (ISSET(svc->runlevels, runlevel))
				inetd_start(&svc->inetd);

			continue;
		}
#endif

		/* All other services consult their callback here */
		svc_dance(svc);
	}

	/* Cleanup stale services */
	svc_clean_dynamic(service_unregister);

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
 * service_register - Register service, task or run commands
 * @type:     %SVC_TYPE_SERVICE(0), %SVC_TYPE_TASK(1), %SVC_TYPE_RUN(2)
 * @line:     A complete command line with -- separated description text
 * @mtime:    The modification time if service is loaded from /etc/finit.d
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
 *     service @username [!0-6,S] /path/to/daemon arg       -- Description
 *     task @username [!0-6,S] /path/to/task arg            -- Description
 *     run  @username [!0-6,S] /path/to/cmd arg             -- Description
 *     inetd tcp/ssh nowait [2345] @root:root /sbin/sshd -i -- Description
 *
 * If the username is left out the command is started as root.  The []
 * brackets denote the allowed runlevels, if left out the default for a
 * service is set to [2-5].  Allowed runlevels mimic that of SysV init
 * with the addition of the 'S' runlevel, which is only run once at
 * startup.  It can be seen as the system bootstrap.  If a task or run
 * command is listed in more than the [S] runlevel they will be called
 * when changing runlevel.
 *
 * For multiple instances of the same command, e.g. multiple DHCP
 * clients, the user must enter an ID, using the :ID syntax.
 *
 *     service :1 /sbin/udhcpc -i eth1
 *     service :2 /sbin/udhcpc -i eth2
 *
 * Without the :ID syntax Finit will overwrite the first service line
 * with the contents of the second.  The :ID must be [1,MAXINT].
 *
 * Returns:
 * POSIX OK(0) on success, or non-zero errno exit status on failure.
 */
int service_register(int type, char *line, time_t mtime, char *username)
{
	int i = 0;
	int id = 1;		/* Default to ID:1 */
#ifndef INETD_DISABLED
	int forking = 0;
#endif
	char *service = NULL, *proto = NULL, *ifaces = NULL;
	char *cmd, *desc, *runlevels = NULL;
	svc_t *svc;
	plugin_t *plugin = NULL;

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
			service = cmd;   /* inetd service/proto */
#ifndef INETD_DISABLED
		else if (!strncasecmp(cmd, "nowait", 6))
			forking = 1;
		else if (!strncasecmp(cmd, "wait", 4))
			forking = 0;
#endif
		else if (cmd[0] == '@')	/* @username[:group] */
			username = &cmd[1];
		else if (cmd[0] == '[')	/* [runlevels] */
			runlevels = &cmd[0];
		else if (cmd[0] == ':')	/* :ID */
			id = atoi(&cmd[1]);
		else
			break;

		/* Check if valid command follows... */
		cmd = strtok(NULL, " ");
		if (!cmd)
			goto incomplete;
	}

	/* Example: inetd ssh/tcp@eth0,eth1 or 222/tcp@eth2 */
	if (service) {
		ifaces = strchr(service, '@');
		if (ifaces)
			*ifaces++ = 0;

		proto = strchr(service, '/');
		if (!proto)
			goto incomplete;
		*proto++ = 0;
	}

#ifndef INETD_DISABLED
	/* Find plugin that provides a callback for this inetd service */
	if (type == SVC_TYPE_INETD) {
		if (!strncasecmp(cmd, "internal", 8)) {
			char *ptr, *ps = service;

			/* internal.service */
			ptr = strchr(cmd, '.');
			if (ptr) {
				*ptr++ = 0;
				ps = ptr;
			}

			plugin = plugin_find(ps);
			if (!plugin || !plugin->inetd.cmd) {
				_e("Inetd service %s has no internal plugin, skipping.", service);
				return errno = ENOENT;
			}
		}

		/* Check if known inetd, then add ifnames for filtering only. */
		svc = find_inetd_svc(cmd, service, proto);
		if (svc)
			goto inetd_setup;

		id = svc_next_id(cmd);
	}
#endif

	svc = svc_find(cmd, id);
	if (!svc) {
		_d("Creating new svc for %s id #%d type %d", cmd, id, type);
		svc = svc_new(cmd, id, type);
		if (!svc) {
			_e("Out of memory, cannot register service %s", cmd);
			return errno = ENOMEM;
		}
	}
	if (svc->mtime != mtime)
		svc->dirty = 1;
	svc->mtime = mtime;

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

	if (plugin) {
		/* Internal plugin provides this service */
		svc->inetd.cmd = plugin->inetd.cmd;
	} else {
		strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));
		while ((cmd = strtok(NULL, " ")))
			strlcpy(svc->args[i++], cmd, sizeof(svc->args[0]));
		svc->args[i][0] = 0;

		plugin = plugin_find(svc->cmd);
		if (plugin && plugin->svc.cb) {
			svc->cb           = plugin->svc.cb;
			svc->dynamic      = plugin->svc.dynamic;
			svc->dynamic_stop = plugin->svc.dynamic_stop;
		}
	}

	svc->runlevels = parse_runlevels(runlevels);
	_d("Service %s runlevel 0x%2x", svc->cmd, svc->runlevels);

#ifndef INETD_DISABLED
	if (svc_is_inetd(svc)) {
		char *iface, *name = service;

		if (svc->inetd.cmd && plugin)
			name = plugin->name;

		if (inetd_new(&svc->inetd, name, service, proto, forking, svc)) {
			_e("Failed registering new inetd service %s.", service);
			inetd_del(&svc->inetd);
			return svc_del(svc);
		}

	inetd_setup:
		if (!ifaces) {
			_d("No specific iface listed for %s, allowing ANY.", service);
			return inetd_allow(&svc->inetd, NULL);
		}

		for (iface = strtok(ifaces, ","); iface; iface = strtok(NULL, ",")) {
			if (iface[0] == '!')
				inetd_deny(&svc->inetd, &iface[1]);
			else
				inetd_allow(&svc->inetd, iface);
		}
	}
#endif

	return 0;
}

void service_unregister(svc_t *svc)
{
	service_stop(svc);
	svc_del(svc);
}

void service_monitor(pid_t lost)
{
	svc_t *svc;
	static int was_stopped = 0;

	if (was_stopped && !is_norespawn()) {
		was_stopped = 0;
		restart_lost_procs();
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

#ifndef INETD_DISABLED
	if (inetd_respawn(lost))
		return;
#endif

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (lost != svc->pid)
			continue;

		if (SVC_TYPE_SERVICE != svc->type) {
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

		/* Restarting lost service. */
		if (service_enabled(svc, 0, NULL)) {
			if (svc->restart_counter > RESPAWN_MAX) {
				_e("Not restarting %s id %d, respawn MAX (%d) reached!",
				   svc->cmd, svc->id, RESPAWN_MAX);
				break;
			}

			svc->restart_counter++;
			service_start(svc);
		}

		break;
	}
}

static int is_norespawn(void)
{
	return  sig_stopped()            ||
		fexist("/mnt/norespawn") ||
		fexist("/tmp/norespawn");
}

static void restart_lost_procs(void)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->pid > 0 && pid_alive(svc->pid))
			continue;

		/* Only restart lost daemons, not task/run/inetd services */
		if (SVC_TYPE_SERVICE != svc->type) {
			svc->pid = 0;
			continue;
		}

		service_start(svc);
	}
}

/* Singing and dancing ... */
static void svc_dance(svc_t *svc)
{
	svc_cmd_t cmd = service_enabled(svc, 0, NULL);

	if (svc->pid) {
		if (SVC_STOP == cmd)
			service_stop(svc);
		else if (SVC_RELOAD == cmd)
			service_reload(svc);
	} else {
		if (SVC_START == cmd || SVC_RELOAD == cmd)
			service_start(svc);
	}
}

#ifndef INETD_DISABLED
static svc_t *find_inetd_svc(char *path, char *service, char *proto)
{
	svc_t *svc;

	for (svc = svc_inetd_iterator(1); svc; svc = svc_inetd_iterator(0)) {
		if (strncmp(path, svc->cmd, strlen(svc->cmd)))
			continue;

		if (inetd_match(&svc->inetd, service, proto)) {
			_d("Found a matching inetd svc for %s %s %s", path, service, proto);
			return svc;
		}
	}

	return NULL;
}
#endif

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
