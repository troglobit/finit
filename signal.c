/*
  Improved fast init

  Copyright (c) 2008 Claudio Matsuoka

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <signal.h>
#include <sys/reboot.h>

#include "finit.h"
#include "helpers.h"

#define SETSIG(sa, sig, fun, flags)			\
	do {						\
		sa.sa_sigaction = fun;			\
		sa.sa_flags = SA_SIGINFO | flags;	\
		sigemptyset(&sa.sa_mask);		\
		sigaction(sig, &sa, NULL);		\
	} while (0)

#define IGNSIG(sa, sig, flags)			\
	do {					\
		sa.sa_handler = SIG_IGN;	\
		sa.sa_flags = flags;		\
		sigemptyset(&sa.sa_mask);	\
		sigaction(sig, &sa, NULL);	\
	} while (0)

static int stopped = 0;
extern char *sdown;

void do_shutdown (int sig)
{
	touch(SYNC_SHUTDOWN);
	if (sdown[0] != 0) {
		system(sdown);
	}

	kill(-1, SIGTERM);

	write(1, "\033[?25l\033[30;40m", 14);
	copyfile("/boot/shutdown.fb", "/dev/fb/0", 0);
	sleep(2);

	system("/usr/sbin/alsactl store > /dev/null 2>&1");
	system("/sbin/hwclock --systohc");

	kill(-1, SIGKILL);

	sync();
	sync();
	system("/bin/umount -a;/bin/mount -n -o remount,ro /");
	//system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");

	if (sig == SIGINT || sig == SIGUSR1)
		reboot(RB_AUTOBOOT);

	reboot(RB_POWER_OFF);
}

/*
 * Shut down on INT USR1 USR2
 */
static void shutdown_handler(int sig, siginfo_t *info, void *ctx __attribute__ ((unused)))
{
	_d("finit: Rebooting on signal %d from pid %d with code %d", sig, info->si_pid, info->si_code);

	do_shutdown(sig);
}

/*
 * SIGCHLD: one of our children has died
 */
static void chld_handler(int sig, siginfo_t *info, void *ctx __attribute__ ((unused)))
{
	int status;

	_d("finit: Child died, harvesting... signal %d from pid %d with code %d", sig, info->si_pid, info->si_code);
	while (waitpid(-1, &status, WNOHANG) != 0) {
		if (errno == ECHILD)
			break;
	}
}

/*
 * SIGSTOP: Paused by user or netflash
 */
static void sigstop_handler(int sig, siginfo_t *info, void *ctx __attribute__ ((unused)))
{
	_d("finit: Received SIGSTOP(%d) from pid %d with code %d", sig, info->si_pid, info->si_code);
	touch(SYNC_STOPPED);
	stopped ++;
}
static void sigcont_handler(int sig, siginfo_t *info, void *ctx __attribute__ ((unused)))
{
	_d("finit: Received SIGCONT(%d) from pid %d with code %d", sig, info->si_pid, info->si_code);
	stopped = 0;
	remove(SYNC_STOPPED);
}

/*
 * Signal management - Be conservative with what finit responds to!
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
void sig_init(void)
{
	int i;
	sigset_t nmask;
	struct sigaction sa;

	for (i = 1; i < NSIG; i++)
		IGNSIG(sa, i, SA_RESTART);

	SETSIG(sa, SIGINT,  shutdown_handler, 0); /* Standard SysV init calls ctrl-alt-delete handler */
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
	SETSIG(sa, SIGCHLD, chld_handler, SA_RESTART);

	/* Block SIGCHLD while forking */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, NULL);

	/* Disable CTRL-ALT-DELETE from kernel, we handle shutdown gracefully with SIGINT */
	reboot(RB_DISABLE_CAD);
	setsid();
}

/**
 * Local Variables:
 *  version-control: t
 *  c-file-style: "ellemtel"
 * End:
 */
