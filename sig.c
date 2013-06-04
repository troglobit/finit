/* Signal management - Be conservative with what finit responds to!
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

#include <sys/reboot.h>
#include <sys/wait.h>

#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "private.h"
#include "sig.h"

static int stopped = 0;

void do_shutdown (int sig)
{
	touch(SYNC_SHUTDOWN);

	if (sdown)
		run_interactive(sdown, "Calling shutdown hook: %s", sdown);

	/* Call all shutdown hooks before rebooting... */
	plugin_run_hooks(HOOK_SHUTDOWN);

	/* Here is where we signal watchdogd to do a forced reset for us */
	_d("Sending SIGTERM to all processes.");
	kill(-1, SIGTERM);

	/* Wait for WDT to timeout, should be no more than ~1 sec. */
	do_sleep(2);

	_d("Sending SIGKILL to remaining processes.");
	kill(-1, SIGKILL);

	sync();
	sync();
	_d("Unmounting file systems, remounting / read-only.");
	run("/bin/umount -fa 2>/dev/null");
	run("/bin/mount -n -o remount,ro / 2>/dev/null");
	run("/sbin/swapoff -ea");

	_d("%s.", sig == SIGINT || sig == SIGUSR1 ? "Rebooting" : "Halting");
	if (sig == SIGINT || sig == SIGUSR1)
		reboot(RB_AUTOBOOT);

	reboot(RB_POWER_OFF);
}

/*
 * Shut down on INT USR1 USR2
 */
static void shutdown_handler(int sig, siginfo_t *info, void *UNUSED(ctx))
{
	_d("Rebooting on signal %d from %s (PID %d)",
	   sig, pid_get_name(info->si_pid, NULL, 0), info->si_pid);

	do_shutdown(sig);
}

/*
 * SIGCHLD: one of our children has died
 */
static void chld_handler(int UNUSED(sig), siginfo_t *UNUSED(info), void *UNUSED(ctx))
{
	/* Do nothing, the svc_monitor() is the designated child reaper. */
}

/*
 * SIGSTOP: Paused by user or netflash
 */
static void sigstop_handler(int sig, siginfo_t *info, void *UNUSED(ctx))
{
	_d("Received SIGSTOP(%d) from %s (PID %d)",
	   sig, pid_get_name(info->si_pid, NULL, 0), info->si_pid);
	touch(SYNC_STOPPED);
	stopped ++;
}
static void sigcont_handler(int sig, siginfo_t *info, void *UNUSED(ctx))
{
	_d("Received SIGCONT(%d) from %s (PID %d)",
	   sig, pid_get_name(info->si_pid, NULL, 0), info->si_pid);
	stopped = 0;
	remove(SYNC_STOPPED);
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
void sig_init(void)
{
	int i;
	struct sigaction sa;

	for (i = 1; i < NSIG; i++)
		IGNSIG(sa, i, SA_RESTART);

	SETSIG(sa, SIGCHLD, chld_handler, SA_RESTART);
}

/*
 * Setup limited set of SysV compatible signals to respond to
 */
void sig_setup(void)
{
	struct sigaction sa;

	_d("Setup signals");

	/* Cleanup any stale finit control files */
	remove(SYNC_SHUTDOWN);
	remove(SYNC_STOPPED);

	/* Standard SysV init calls ctrl-alt-delete handler */
	SETSIG(sa, SIGINT,  shutdown_handler, 0);
	SETSIG(sa, SIGPWR,  shutdown_handler, 0);

	/* Ignore SIGUSR1/2 for now, only BusyBox init implements them as reboot+halt. */
//	  SETSIG2(sa, SIGUSR1, reopen_initctl, 0);
//	  SETSIG2(sa, SIGUSR2, pwrdwn_handler, 0);

	/* Init must ignore SIGTERM. May otherwise get false SIGTERM in forked children! */
	IGNSIG(sa, SIGTERM, 0);

	/* Some C APIs may need SIGALRM for implementing timers. */
	IGNSIG(sa, SIGALRM, 0);

	/* We don't have any /etc/inittab yet, reread finit.conf? */
	IGNSIG(sa, SIGHUP, 0);

	/* Stopping init is a bit tricky. */
	SETSIG(sa, SIGSTOP, sigstop_handler, 0);
	SETSIG(sa, SIGTSTP, sigstop_handler, 0);
	SETSIG(sa, SIGCONT, sigcont_handler, 0);

	/* Disable CTRL-ALT-DELETE from kernel, we handle shutdown gracefully with SIGINT, above */
	reboot(RB_DISABLE_CAD);
	setsid();
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
