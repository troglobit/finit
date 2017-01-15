/* Signal management - Be conservative with what finit responds to!
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2014  Joachim Nilsson <troglobit@gmail.com>
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
 *      Same effect as init/telinit q, reloads *.conf in /etc/finit.d/
 *
 * SIGUSR1
 *      Calls shutdown hooks, including HOOK_SHUTDOWN, stops all running
 *      processes, and unmounts all file systems.  Then tells kernel to
 *      halt.  Most people these days want SIGUSR2 though.
 *
 *      (SysV init and systemd use this to re-open their FIFO/D-Bus.)
 *
 * SIGUSR2
 *      Like SIGUSR1, but tell kernel to power-off the system, if ACPI
 *      or similar exists to actually do this.  If the kernel fails
 *      power-off, Finit falls back to halt.
 *
 *      (SysV init N/A, systemd dumps its internal state to log.)
 *
 * SIGTERM
 *      Like SIGUSR1, but tell kernel to reboot the system when done.
 *
 *      (SysV init N/A, systemd rexecutes itself.)
 *
 * SIGINT
 *      Sent from kernel when the CTRL-ALT-DEL key combo is pressed.
 *      SysV init and systemd default to reboot with `shutdown -r`.
 *      Finit currently forwards this to SIGTERM.
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
 *      Finit currently forwards this to SIGUSR2.
 */

#include <dirent.h>
#include <string.h>		/* strerror() */
#include <sys/reboot.h>
#include <sys/wait.h>
#include <lite/lite.h>

#include "finit.h"
#include "conf.h"
#include "config.h"
#include "helpers.h"
#include "plugin.h"
#include "private.h"
#include "sig.h"
#include "service.h"
#include "util.h"
#include "utmp-api.h"

/*
 * Old-style SysV shutdown sends a setenv cmd INIT_HALT with "=HALT",
 * "=POWERDOWN", or "" to cancel shutdown, before requesting change to
 * runlevel 6 over the /dev/initctl FIFO.  See plugins/initctl.c
 */
shutop_t halt = SHUT_DEFAULT;

static int   stopped = 0;
static uev_t sigterm_watcher, sigusr1_watcher, sigusr2_watcher;
static uev_t sighup_watcher,  sigint_watcher,  sigpwr_watcher;
static uev_t sigchld_watcher;
static uev_t sigstop_watcher, sigtstp_watcher, sigcont_watcher;

void mdadm_wait(void);
void unmount_tmpfs(void);
void unmount_regular(void);

/*
 * Kernel threads have no cmdline so fgets() returns NULL for them.  We
 * also skip "special" processes, e.g. mdadm/mdmon or watchdogd that must
 * not be stopped here, for various reasons.
 *
 * https://www.freedesktop.org/wiki/Software/systemd/RootStorageDaemons/
 */
void do_kill(int signo)
{
	DIR *dirp;

	dirp = opendir("/proc");
	if (dirp) {
		struct dirent *d;

		while ((d = readdir(dirp))) {
			int pid;
			FILE *fp;
			char file[80] = "";

			if (d->d_type != DT_DIR)
				continue;

			pid = atoi(d->d_name);
			if (!pid)
				continue;

			snprintf(file, sizeof(file), "/proc/%s/cmdline", d->d_name);
			fp = fopen(file, "r");
			if (!fp)
				continue;

			if (fgets(file, sizeof(file), fp)) {
				if (file[0] != '@')
					kill(pid, signo);
			}
			fclose(fp);
		}
		closedir(dirp);
	}
}

void do_shutdown(shutop_t op)
{
	touch(SYNC_SHUTDOWN);

	if (sdown)
		run_interactive(sdown, "Calling shutdown hook: %s", sdown);

	/* Update UTMP db */
	utmp_set_halt();

	/*
	 * Tell all remaining non-monitored processes to exit, give them
	 * some time to exit gracefully, 2 sec is customary.
	 */
	do_kill(SIGTERM);
	do_sleep(2);
	do_kill(SIGKILL);

	/* Exit plugins and API gracefully */
	plugin_exit();
	api_exit();

	/* Reap 'em */
	while (waitpid(-1, NULL, WNOHANG) > 0)
		;

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

	/* Unmount any tmpfs before unmounting swap ... */
	unmount_tmpfs();
	run("/sbin/swapoff -e -a");

	/* ... unmount remaining regular file systems. */
	unmount_regular();

	/* We sit on / so we must remount it ro, try all the things! */
	sync();
	run("/bin/mount -n -o remount,ro -t dummytype dummydev /");
	run("/bin/mount -n -o remount,ro dummydev /");
	run("/bin/mount -n -o remount,ro /");

	/* Call mdadm to mark any RAID array(s) as clean before halting. */
	mdadm_wait();

	/* Reboot via watchdog or kernel, or shutdown? */
	if (op == SHUT_REBOOT) {
		if (wdogpid) {
			int timeout = 10;

			/* Wait here until the WDT reboots, or timeout with fallback */
			print(kill(wdogpid, SIGPWR) == 1, "Pending watchdog reboot");
			while (timeout--)
				do_sleep(1);
		}

		_d("Rebooting ...");
		reboot(RB_AUTOBOOT);
	} else if (op == SHUT_OFF) {
		_d("Powering down ...");
		reboot(RB_POWER_OFF);
	}

	/* Also fallback if any of the other two fails */
	_d("Halting ...");
	reboot(RB_HALT_SYSTEM);
}

/*
 * Reload .conf files in /etc/finit.d/
 */
static void sighup_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_d("...");
	/* INIT_CMD_RELOAD: 'init q', 'initctl reload', and SIGHUP */
	service_reload_dynamic();
}

/*
 * SIGINT: Should generate <sys/key/ctrlaltdel> condition, for now reboot
 */
static void sigint_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_d("...");
	halt = SHUT_REBOOT;
	service_runlevel(6);
}

/*
 * SIGUSR1: BusyBox style halt
 */
static void sigusr1_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_d("...");
	halt = SHUT_HALT;
	service_runlevel(0);
}

/*
 * SIGUSR2: BusyBox style poweroff
 */
static void sigusr2_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_d("...");
	halt = SHUT_OFF;
	service_runlevel(0);
}

/*
 * SIGTERM: BusyBox style reboot
 */
static void sigterm_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_d("...");
	halt = SHUT_REBOOT;
	service_runlevel(6);
}

/*
 * SIGCHLD: one of our children has died
 */
static void sigchld_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	pid_t pid;

	/* Reap all the children! */
	do {
		pid = waitpid(-1, NULL, WNOHANG);
		if (pid > 0) {
			_d("Collected child %d", pid);
			service_monitor(pid);
		}
	} while (pid > 0);
}

/*
 * SIGSTOP/SIGTSTP: Paused by user or netflash
 */
static void sigstop_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	touch(SYNC_STOPPED);
	stopped++;
}

/*
 * SIGCONT: Restart service monitor
 */
static void sigcont_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	stopped = 0;
	erase(SYNC_STOPPED);
}

/*
 * Is SIGSTOP asserted?
 */
int sig_stopped(void)
{
	return stopped;
}

/*
 * Inital signal setup - ignore everything but SIGCHLD until we're capable of responding
 */
static void chld_handler(int UNUSED(sig), siginfo_t *UNUSED(info), void *UNUSED(ctx))
{
	/* NOP */
}

void sig_init(void)
{
	int i;
	struct sigaction sa;

	for (i = 1; i < NSIG; i++)
		IGNSIG(sa, i, SA_RESTART);

	SETSIG(sa, SIGCHLD, chld_handler, SA_RESTART);
}

/*
 * Unblock all signals blocked by finit when starting children
 */
void sig_unblock(void)
{
	int i;
	sigset_t nmask;
	struct sigaction sa;

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

	_d("Setup signals");

	/* Cleanup any stale finit control files */
	erase(SYNC_SHUTDOWN);
	erase(SYNC_STOPPED);

	/* Standard SysV init calls ctrl-alt-delete handler */
	uev_signal_init(ctx, &sigint_watcher, sigint_cb, NULL, SIGINT);
	uev_signal_init(ctx, &sigpwr_watcher, sigint_cb, NULL, SIGPWR);

	/* BusyBox init style signals for halt, power-off and reboot. */
	uev_signal_init(ctx, &sigusr1_watcher, sigusr1_cb, NULL, SIGUSR1);
	uev_signal_init(ctx, &sigusr2_watcher, sigusr2_cb, NULL, SIGUSR2);
	uev_signal_init(ctx, &sigterm_watcher, sigterm_cb, NULL, SIGTERM);

	/* Some C APIs may need SIGALRM for implementing timers. */
	IGNSIG(sa, SIGALRM, 0);

	/* /etc/inittab not supported yet, instead /etc/finit.d/ is scanned for *.conf */
	uev_signal_init(ctx, &sighup_watcher, sighup_cb, NULL, SIGHUP);

	/* After initial bootstrap of Finit we call the service monitor to reap children */
	uev_signal_init(ctx, &sigchld_watcher, sigchld_cb, NULL, SIGCHLD);

	/* Stopping init is a bit tricky. */
	uev_signal_init(ctx, &sigstop_watcher, sigstop_cb, NULL, SIGSTOP);
	uev_signal_init(ctx, &sigtstp_watcher, sigstop_cb, NULL, SIGTSTP);
	uev_signal_init(ctx, &sigcont_watcher, sigcont_cb, NULL, SIGCONT);

	setsid();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
