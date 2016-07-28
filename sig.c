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
 * The standard SysV init only responds to the following signals:
 *	 SIGHUP
 *	      Has the same effect as telinit q.
 *
 *	 SIGUSR1
 *	      On receipt of this signals, init closes and re-opens  its	 control
 *	      fifo, /dev/initctl. Useful for bootscripts when /dev is remounted.
 *
 *	 SIGINT
 *	      Normally the kernel sends this signal to init when CTRL-ALT-DEL is
 *	      pressed. It activates the ctrlaltdel action.
 *
 *	 SIGWINCH
 *	      The kernel sends this signal when the KeyboardSignal key	is  hit.
 *	      It activates the kbrequest action.
 */

#include <sched.h>
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

static int   stopped = 0;
static uev_t sighup_watcher, sigint_watcher, sigpwr_watcher;
static uev_t sigchld_watcher, sigsegv_watcher;
static uev_t sigstop_watcher, sigtstp_watcher, sigcont_watcher;


void do_shutdown(int sig)
{
	touch(SYNC_SHUTDOWN);

	if (sdown)
		run_interactive(sdown, "Calling shutdown hook: %s", sdown);

	/* If we enabled terse mode at boot, restore to previous setting at shutdown */
	if (quiet) {
		silent = SILENT_MODE;
		if (!silent) {
			sched_yield();
			fputs("\n", stderr);
		}
	}

	/* Call all shutdown hooks before rebooting... */
	plugin_run_hooks(HOOK_SHUTDOWN);

	/* Here is where we signal watchdogd to do a forced reset for us */
	_d("Sending SIGTERM to all processes.");
	kill(-1, SIGTERM);

	/* Wait for WDT to timeout, should be no more than ~1 sec. */
	do_sleep(2);

	_d("Sending SIGKILL to remaining processes.");
	kill(-1, SIGKILL);

	debug = 1;
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
	run("/bin/umount -r -a");
	run("/sbin/swapoff -e -a");

	/* Now, unmount everything else ... err, just force it. */
	run("/bin/umount -n -f -r -a");

	/* We sit on / so we must remount it ro, try all the things! */
	run("/bin/mount -n -o remount,ro -t dummytype dummydev /");
	run("/bin/mount -n -o remount,ro dummydev /");
	run("/bin/mount -n -o remount,ro /");

	_e("PID %d: %s ...", getpid(), sig == SIGINT || sig == SIGUSR1 ? "Rebooting" : "Halting");
	do_sleep(5);

	_d("%s.", sig == SIGINT || sig == SIGUSR1 ? "Rebooting" : "Halting");
	if (sig == SIGINT || sig == SIGUSR1)
		reboot(RB_AUTOBOOT);

	reboot(RB_POWER_OFF);
	reboot(RB_HALT_SYSTEM);	/* Eh, what?  Should not get here ... */
}

/*
 * Reload .conf files in /etc/finit.d/
 */
static void sighup_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	/* INIT_CMD_RELOAD: 'init q', 'initctl reload', and SIGHUP */
	service_reload_dynamic();
}

/*
 * Shut down on INT PWR
 */
static void sigint_cb(uev_t *w, void *UNUSED(arg), int UNUSED(events))
{
	do_shutdown(w->signo);
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
 * SIGSEGV: mostly if service callbacks segfault
 */
static void sigsegv_cb(uev_t *UNUSED(w), void *UNUSED(arg), int UNUSED(events))
{
	_e("PID %d caused a segfault!\n", getpid());
	exit(-1);
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

	/* Ignore SIGUSR1/2 for now, only BusyBox init implements them as reboot+halt. */
//	uev_signal_init(&ctx, &sigusr1_watcher, reopen_initctl_cb, NULL, SIGUSR1);
//	uev_signal_init(&ctx, &sigusr2_watcher, pwrdwn_cb, NULL, SIGUSR2);

	/* Init must ignore SIGTERM. May otherwise get false SIGTERM in forked children! */
	IGNSIG(sa, SIGTERM, 0);

	/* Some C APIs may need SIGALRM for implementing timers. */
	IGNSIG(sa, SIGALRM, 0);

	/* /etc/inittab not supported yet, instead /etc/finit.d/ is scanned for *.conf */
	uev_signal_init(ctx, &sighup_watcher, sighup_cb, NULL, SIGHUP);

	/* After initial bootstrap of Finit we call the service monitor to reap children */
	uev_signal_init(ctx, &sigchld_watcher, sigchld_cb, NULL, SIGCHLD);

	/* Trap SIGSEGV in case service callbacks crash */
	uev_signal_init(ctx, &sigsegv_watcher, sigsegv_cb, NULL, SIGSEGV);

	/* Stopping init is a bit tricky. */
	uev_signal_init(ctx, &sigstop_watcher, sigstop_cb, NULL, SIGSTOP);
	uev_signal_init(ctx, &sigtstp_watcher, sigstop_cb, NULL, SIGTSTP);
	uev_signal_init(ctx, &sigcont_watcher, sigcont_cb, NULL, SIGCONT);

	/* Disable CTRL-ALT-DELETE from kernel, we handle shutdown gracefully with SIGINT, above */
	reboot(RB_DISABLE_CAD);
	setsid();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
