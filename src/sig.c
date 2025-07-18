/* Signal management - Be conservative with what finit responds to!
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2025  Joachim Wiberg <troglobit@gmail.com>
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
 *
 * Finit is often used on embedded and small Linux systems with BusyBox.
 * For compatibility with its existing toolset (halt, poweroff, reboot),
 * the following signals have been adopted:
 *
 * SIGHUP
 *      Same effect as initctl reload, reloads *.conf in /etc/finit.d/
 *      Also restarts API (initctl) socket, like SysV init and systemd
 *      does on USR1 with their FIFO/D-Bus.
 *
 * SIGUSR1
 *      Restarts API (initctl) socket, like SysV init and systemd does
 *      on USR1 with their FIFO/D-Bus.
 *
 *      Finit <= 4.0 used to perform a system halt on USR1.  This caused
 *      some quite nasty problems on systems with both systemd/sysvinit
 *      installed alongside finit.  For compatibility reasons Finit 4.1
 *      changed to partial SIGHUP (api_exit/init).
 *
 * SIGUSR2
 *      Calls shutdown hooks, including HOOK_SHUTDOWN, stops all running
 *      processes, and unmounts all file systems.  Then tells kernel to
 *      power-off the system, if ACPI or similar exists to actually do
 *      this.  If the kernel fails power-off, Finit falls back to halt.
 *
 *      (SysV init N/A, systemd dumps its internal state to log.)
 *
 * SIGTERM
 *      Like SIGUSR2, but tell kernel to reboot the system when done.
 *
 *      (SysV init N/A, systemd rexecutes itself.)
 *
 * SIGINT
 *      Sent from kernel when the CTRL-ALT-DEL key combo is pressed.
 *      SysV init and systemd default to reboot with `shutdown -r`.
 *      Finit defaults to nothing but sets sys/key/ctrlaltdel condition.
 *
 * SIGWINCH
 *      Sent from kernel when the KeyboardSignal key is pressed.  Some
 *      users bind keys, see loadkeys(1), to create consoles on demand.
 *      Finit currently ignores this signal.
 *
 * SIGPWR
 *      Sent from a power daemon, like powstatd(8), on changes to the
 *      UPS status.  Traditionally SysV init read /etc/powerstatus and
 *      acted on "OK", "FAIL", or "LOW" and then removed the file.
 *      Finit  defaults to nothing but sets sys/pwr/fail condition.
 */

#include "config.h"		/* Generated by configure script */

#include <dirent.h>
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <string.h>		/* strerror() */
#include <sched.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "finit.h"
#include "cond.h"
#include "conf.h"
#include "config.h"
#include "helpers.h"
#include "plugin.h"
#include "private.h"
#include "sig.h"
#include "service.h"
#include "sm.h"
#include "util.h"
#include "utmp-api.h"

/*
 * Old-style SysV shutdown sends a setenv cmd INIT_HALT with "=HALT",
 * "=POWERDOWN", or "" to cancel shutdown, before requesting change to
 * runlevel 6 over the /dev/initctl FIFO.
 */
shutop_t halt = SHUT_DEFAULT;

static uev_t sigterm_watcher, sigusr1_watcher, sigusr2_watcher;
static uev_t sighup_watcher,  sigint_watcher,  sigpwr_watcher;
static uev_t sigchld_watcher;

static struct sigmap {
	int   num;
	char *name;
} signames[] = {
	/* ISO C99 signals.  */
	{ SIGINT,    "SIGINT"    },
	{ SIGILL,    "SIGILL"    },
	{ SIGABRT,   "SIGABRT"   },
	{ SIGFPE,    "SIGFPE"    },
	{ SIGSEGV,   "SIGSEGV"   },
	{ SIGTERM,   "SIGTERM"   },
	/* Historical signals specified by POSIX. */
	{ SIGHUP,    "SIGHUP"    },
	{ SIGQUIT,   "SIGQUIT"   },
	{ SIGTRAP,   "SIGTRAP"   },
	{ SIGKILL,   "SIGKILL"   },
	{ SIGBUS,    "SIGBUS"    },
	{ SIGSYS,    "SIGSYS"    },
	{ SIGPIPE,   "SIGPIPE"   },
	{ SIGALRM,   "SIGALRM"   },

	/* New(er) POSIX signals (1003.1-2008, 1003.1-2013). */
	{ SIGURG,    "SIGURG"    },
	{ SIGSTOP,   "SIGSTOP"   },
	{ SIGTSTP,   "SIGTSTP"   },
	{ SIGCONT,   "SIGCONT"   },
	{ SIGCHLD,   "SIGCHLD"   },
	{ SIGTTIN,   "SIGTTIN"   },
	{ SIGTTOU,   "SIGTTOU"   },
	{ SIGPOLL,   "SIGPOLL"   },
	{ SIGXCPU,   "SIGXCPU"   },
	{ SIGXFSZ,   "SIGXFSZ"   },
	{ SIGVTALRM, "SIGVTALRM" },
	{ SIGPROF,   "SIGPROF"   },
	{ SIGUSR1,   "SIGUSR1"   },
	{ SIGUSR2,   "SIGUSR2"   },

	/* Nonstandard signals found in all modern POSIX systems
	   (including both BSD and Linux).  */
	{ SIGWINCH,  "SIGWINCH"  },
#ifdef SIGSTKFLT
	{ SIGSTKFLT, "SIGSTKFLT" },
#endif
	{ SIGPWR,    "SIGPWR"    },

	/* Archaic names for compatibility.  */
#ifdef SIGIO
	{ SIGIO,     "SIGIO"     },
#endif
#ifdef SIGIOT
	{ SIGIOT,    "SIGIOT"    },
#endif
#ifdef SIGCLD
	{ SIGCLD,    "SIGCLD"    },
#endif
};

void mdadm_wait(void);
void unmount_tmpfs(void);
void unmount_regular(void);

static void fs_swapoff(void)
{
	struct mntent *mnt;
	char cmd[256];
	FILE *fp;

	if (!whichp("swapoff"))
		return;

	fp = setmntent(fstab, "r");
	if (!fp)
		return;

	while ((mnt = getmntent(fp))) {
		if (strcmp(mnt->mnt_type, MNTTYPE_SWAP))
			continue;

		snprintf(cmd, sizeof(cmd), "swapoff %s", mnt->mnt_fsname);
		run_interactive(cmd, "Disabling swap %s", mnt->mnt_fsname);
	}

	endmntent(fp);
}

/*
 * Kernel threads have no cmdline so fgets() returns NULL for them.  We
 * also skip "special" processes, e.g. mdadm/mdmon or watchdogd that must
 * not be stopped here, for various reasons.
 *
 * https://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons/
 */
void do_iterate_proc(int (*cb)(int, void *), void *data)
{
	DIR *dirp;

	dirp = opendir("/proc");
	if (dirp) {
		struct dirent *d;

		while ((d = readdir(dirp))) {
			int pid;
			FILE *fp;
			char file[LINE_SIZE] = "";

			if (d->d_type != DT_DIR)
				continue;

			pid = atoi(d->d_name);
			if (!pid)
				continue;
			if (pid == 1)
				continue;

			snprintf(file, sizeof(file), "/proc/%s/cmdline", d->d_name);
			fp = fopen(file, "r");
			if (!fp)
				continue;

			if (fgets(file, sizeof(file), fp)) {
				if (strstr(file, "gdbserver"))
					dbg("Skipping %s ...", file);
				else if (file[0] == '@')
					dbg("Skipping %s ...", &file[1]);
				else if (strstr(file, "initctl"))
					dbg("Skipping %s ...", file);
				else
					if (cb(pid, data)) {
						dbg("PID %d is still alive (%s)", pid, file);
						fclose(fp);
						break;
					}
			}
			fclose(fp);
		}
		closedir(dirp);
	}
}

static int kill_cb(int pid, void *data)
{
	int signo = *(int *)data;

	kill(pid, signo);

	return 0;
}

static int status_cb(int pid, void *data)
{
	int *has_proc = (int *)data;

	(void)pid;
	*has_proc = 1;

	return 1;
}

/* return value:
 *  1 - at least one process remaining
 *  0 - no processes remaining
 */
static int do_wait(int secs)
{
	const int delay = 250000;
	int iterations = secs * 1000 * 1000 / delay;
	int has_proc;

	do {
		do_usleep(delay);
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
		has_proc = 0;
		iterations--;
		do_iterate_proc(status_cb, &has_proc);
	}
	while (has_proc && iterations > 0);

	return has_proc;
}

void do_shutdown(shutop_t op)
{
	struct sched_param sched_param = { .sched_priority = 99 };
	int in_cont = in_container();
	int signo = SIGTERM;

	if (!in_cont) {
		/*
		 * On a PREEMPT-RT system, Finit must run as the highest prioritized
		 * RT process to ensure it completes the shutdown sequence.
		 */
		sched_setscheduler(1, SCHED_RR, &sched_param);
	}

	halt = op;
	if (sdown)
		run_interactive(sdown, "Calling shutdown hook: %s", sdown);

	/* Update UTMP db */
	utmp_set_halt();

	/*
	 * Tell remaining non-monitored processes to exit, give them
	 * time to exit gracefully, 2 sec was customary, we go for 1.
	 */
	do_iterate_proc(kill_cb, &signo);
	if (do_wait(1)) {
		signo = SIGKILL;
		do_iterate_proc(kill_cb, &signo);
	}

	/* Exit plugins gracefully */
	plugin_exit();

	/* We can no longer act on conditions, activate script runner */
	cond_exit();

	/* Reap 'em */
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

	/* All services and (non-critical) processes have stopped. */
	plugin_script_run(HOOK_SVC_DN);

	if (in_cont) {
		if (osheading)
			logit(LOG_CONSOLE | LOG_NOTICE, "%s, shutting down container.", osheading);
		_exit(0);
	}

	/* Close all local non-console descriptors */
	for (int fd = 3; fd < 128; fd++)
		close(fd);

	if (vfork()) {
		/*
		 * Put PID 1 aside and let child perform reboot/halt
		 * kernel may exit child and we don't want to exit PID 1
		 * ... causing "aiii killing init" during reboot ...
		 */
		return;
	}

	if (wdog) {
		print(kill(wdog->pid, SIGPWR) == 1, "Advising watchdog, system going down");
		do_sleep(2);
	}

	/* Unmount any tmpfs before unmounting swap ... */
	print(0, "Unmounting filesystems ...");
	unmount_tmpfs();
	fs_swapoff();

	/* ... unmount remaining regular file systems. */
	unmount_regular();

	/*
	 * We sit on / so we must remount it ro, try all the things!
	 * See the following links for details:
	 *  - https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=339023
	 *  - https://bugs.launchpad.net/ubuntu/+source/util-linux/+bug/29187
	 */
	sync();
	run("mount -n -o remount,ro -t dummytype dummydev /", NULL);
	run("mount -n -o remount,ro dummydev /", NULL);
	run("mount -n -o remount,ro /", "mount");

	/* Call mdadm to mark any RAID array(s) as clean before halting. */
	mdadm_wait();

	/* Last chance to run scripts (all .so plugins have exited already) */
	print(0, "Calling hook/svc/down scripts ...");
	plugin_script_run(HOOK_SYS_DN);

	/*
	 * Optional reboot-delay, needed on systems running ubifs, jffs,
	 * or similar.  From fileutils-4.0, sync(8); "On Linux, sync is
	 * only guaranteed to schedule the dirty blocks for writing; it
	 * can actually take a short time before all the blocks are
	 * finally written." -- https://linux.die.net/man/8/sync
	 */
	sync();
	if (syncsec > 0) {
		print(-1, "Reboot delay, waiting for filesystem sync, %d sec", syncsec);
		do_sleep(syncsec);
		print(0, NULL);
	}

	/* Reboot via watchdog or kernel, or shutdown? */
	if (op == SHUT_REBOOT) {
		print(0, "Rebooting ...");
		if (wdog && wdog->pid > 1) {
			int timeout = 10;

			/* Wait here until the WDT reboots, or timeout with fallback */
			print(kill(wdog->pid, SIGTERM) == 1, "Pending watchdog reboot");
			while (timeout--)
				do_sleep(1);
		}

		dbg("Rebooting ...");
		reboot(RB_AUTOBOOT);
	} else if (op == SHUT_OFF) {
		print(0, "Powering down ...");
		reboot(RB_POWER_OFF);
	}

	/* Also fallback if any of the other two fails */
	print(0, "Halting ...");
	reboot(RB_HALT_SYSTEM);
}

/*
 * Reload .conf files in /etc/finit.d/
 */
static void sighup_cb(uev_t *w, void *arg, int events)
{
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	if (IS_RESERVED_RUNLEVEL(runlevel)) {
		warnx("SIGHUP ignored in runlevel S and 6.");
		return;
	}

	/* Restart initctl API domain socket, similar to systemd/SysV init */
	api_exit();
	api_init(w->ctx);

	/* INIT_CMD_RELOAD: 'init q', 'initctl reload', and SIGHUP */
	sm_reload();
}

/*
 * SIGINT: generates <sys/key/ctrlaltdel> condition, which the sys.so
 *         plugin picks up and tells Finit to start any service(s) or
 *         task(s) associated with the condition.
 */
static void sigint_cb(uev_t *w, void *arg, int events)
{
	(void)w;
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	cond_set_oneshot_noupdate("sys/key/ctrlaltdel");
}

/*
 * SIGPWR: generates <sys/pwr/fail> condition, which the sys.so plugin
 *         picks up and tells Finit to start any service(s) or task(s)
 *         associated with the condition.
 */
static void sigpwr_cb(uev_t *w, void *arg, int events)
{
	(void)w;
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	cond_set_oneshot_noupdate("sys/pwr/fail");
}

/*
 * SIGUSR1: SysV init/systemd API socket restart
 */
static void sigusr1_cb(uev_t *w, void *arg, int events)
{
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	if (IS_RESERVED_RUNLEVEL(runlevel)) {
		warnx("SIGUSR1 ignored in runlevel S and 6.");
		return;
	}

	/* Restart initctl API domain socket, similar to systemd/SysV init */
	api_exit();
	api_init(w->ctx);
}

/*
 * SIGUSR2: BusyBox style poweroff
 */
static void sigusr2_cb(uev_t *w, void *arg, int events)
{
	(void)w;
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	if (IS_RESERVED_RUNLEVEL(runlevel)) {
		warnx("SIGUSR2 ignored in runlevel S and 6.");
		return;
	}

	halt = SHUT_OFF;
	sm_runlevel(0);
}

/*
 * SIGTERM: BusyBox style reboot
 */
static void sigterm_cb(uev_t *w, void *arg, int events)
{
	(void)w;
	(void)arg;

	dbg("...");
	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	if (IS_RESERVED_RUNLEVEL(runlevel)) {
		warnx("SIGTERM ignored in runlevel S and 6.");
		return;
	}

	halt = SHUT_REBOOT;
	sm_runlevel(6);
}

/*
 * SIGCHLD: one of our children has died
 */
static void sigchld_cb(uev_t *w, void *arg, int events)
{
	int status;
	pid_t pid;

	(void)w;
	(void)arg;

	if (UEV_ERROR == events) {
		errx(1, "Unrecoverable error in signal watcher");
		return;
	}

	/* Reap all the children! */
	while (1) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid <= 0) {
			if (pid == -1 && errno == EINTR)
				continue;
			break;
		}

		dbg("Collected child PID %d, status: %d", pid, status);
		service_monitor(pid, status);
	}
}

/*
 * Convert SIGFOO to a number, if it exists
 */
int sig_num(const char *name)
{
	size_t i, offset = 0;

	if (strncasecmp(name, "SIG", 3))
		offset = 3;

	for (i = 0; i < NELEMS(signames); i++) {
		if (strcasecmp(name, &signames[i].name[offset]))
			continue;

		return signames[i].num;
	}

	return -1;
}

const char *sig_name(int signo)
{
	size_t i;

	for (i = 0; i < NELEMS(signames); i++) {
		if (signames[i].num == signo)
			return signames[i].name;
	}

	return "SIGUNKOWN";
}

/*
 * Initial signal setup - ignore everything but SIGCHLD until we're capable of responding
 */
static void chld_handler(int sig, siginfo_t *info, void *ctx)
{
	(void)sig;
	(void)info;
	(void)ctx;
	/* NOP */
}

void sig_init(void)
{
	struct sigaction sa;
	int i;

	for (i = 1; i < NSIG; i++)
		IGNSIG(sa, i, SA_RESTART);

	SETSIG(sa, SIGCHLD, chld_handler, SA_RESTART);
}

/*
 * Unblock all signals blocked by finit when starting children
 */
void sig_unblock(void)
{
	struct sigaction sa;
	sigset_t nmask;
	int i;

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigaddset(&nmask, SIGCHLD);
	sigaddset(&nmask, SIGINT);
	sigaddset(&nmask, SIGPWR);
	sigaddset(&nmask, SIGSTOP);
	sigaddset(&nmask, SIGTSTP);
	sigaddset(&nmask, SIGCONT);
	sigaddset(&nmask, SIGTERM);
	sigaddset(&nmask, SIGUSR1);
	sigaddset(&nmask, SIGUSR2);
	sigprocmask(SIG_UNBLOCK, &nmask, NULL);

	/* Reset signal handlers that were set by the parent process */
	for (i = 1; i < NSIG; i++)
		DFLSIG(sa, i, 0);
}

/*
 * Setup limited set of SysV compatible signals to respond to
 */
void sig_setup(uev_ctx_t *ctx)
{
	struct sigaction sa;

	dbg("Setup signals");

	/*
	 * Standard SysV init calls ctrl-alt-delete handler
	 * We need to disable kernel default so it sends us SIGINT
	 */
	reboot(RB_DISABLE_CAD);
	uev_signal_init(ctx, &sigint_watcher, sigint_cb, NULL, SIGINT);

	/* BusyBox/SysV init style signals for halt, power-off and reboot. */
	uev_signal_init(ctx, &sigusr1_watcher, sigusr1_cb, NULL, SIGUSR1);
	uev_signal_init(ctx, &sigusr2_watcher, sigusr2_cb, NULL, SIGUSR2);
	uev_signal_init(ctx, &sigpwr_watcher,  sigpwr_cb,  NULL, SIGPWR);
	uev_signal_init(ctx, &sigterm_watcher, sigterm_cb, NULL, SIGTERM);

	/* Some C APIs may need SIGALRM for implementing timers. */
	IGNSIG(sa, SIGALRM, 0);

	/* /etc/inittab not supported yet, instead /etc/finit.d/ is scanned for *.conf */
	uev_signal_init(ctx, &sighup_watcher, sighup_cb, NULL, SIGHUP);

	/* After initial bootstrap of Finit we call the service monitor to reap children */
	uev_signal_init(ctx, &sigchld_watcher, sigchld_cb, NULL, SIGCHLD);

	setsid();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
