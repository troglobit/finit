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

/*

Changelog from the original Eeepc fastinit:

- Use mknod() instead of close(open()) call to create empty file
- Fix random seed initialization
- Don't try to handle signal 0, SIGKILL or SIGSTOP
- Implement makepath() to avoid system("mkdir -p")
- Omit first hwclock if user has CONFIG_RTC_HCTOSYS in kernel (by Metalshark)
- Add –directisa to hwclock if user has disabled CONFIG_GENRTC and enabled
  CONFIG_RTC but not CONFIG_HPET_TIMER and CONFIG_HPET_RTC_IRQ (by Metalshark)
- Drop system() call to clean /tmp, it's a fresh mounted tmpfs
- Change /proc/acpi/sleep to /sys/power/state (by Metalshark)
- Set loopback interface using direct calls instead of system("ifconfig")
- Copy 4096 data block in C instead of system("cat") or system("dd")
- Draw shutdown splash screen using C calls instead of system("echo;cat")
- Change poweroff method from writing 5 to /sys/power/state to
  reboot(RB_POWER_OFF) (by Metalshark)
- Mount /var/run and /var/lock as tmpfs
- Use SIG_IGN instead of empty signal handler (by Metalshark)
- Add built-in run-parts option to be used instead of system("run-parts")
- Reuse cmdline buffer as hostname and filename buffers
- Open terminal in vt2 using openvt

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <linux/fs.h>

#include "helpers.h"

#ifdef DIRECTISA
#define HWCLOCK_DIRECTISA " --directisa"
#else
#define HWCLOCK_DIRECTISA
#endif


/* From sysvinit */
/* Set a signal handler. */
#define SETSIG(sa, sig, fun, flags) \
		do { \
			sa.sa_handler = fun; \
			sa.sa_flags = flags; \
			sigemptyset(&sa.sa_mask); \
			sigaction(sig, &sa, NULL); \
		} while(0)

#define touch(x) mknod((x), S_IFREG|0644, 0)

#define LINE_SIZE 1024

void shutdown(int);
void chld_handler(int);


int main()
{
	int i;
	FILE *f;
	char line[LINE_SIZE];
	int fd;
	DIR *dir;
	struct dirent *d;
	struct sigaction sa, act;
	char *x;
	sigset_t nmask, nmask2;

	puts("finit-mod " VERSION " (built " __DATE__ " by " WHOAMI ")");

	chdir("/");
	umask(022);
	
	/*
	 * Signal management
	 */
	for (i = 1; i < NSIG; i++)
		SETSIG(sa, i, SIG_IGN, SA_RESTART);

	SETSIG(sa, SIGINT,  shutdown, 0);
	SETSIG(sa, SIGPWR,  SIG_IGN, 0);
	SETSIG(sa, SIGUSR1, shutdown, 0);
	SETSIG(sa, SIGUSR2, shutdown, 0);
	SETSIG(sa, SIGTERM, SIG_IGN, 0);
	SETSIG(sa, SIGALRM, SIG_IGN, 0);
	SETSIG(sa, SIGHUP,  SIG_IGN, 0);
	SETSIG(sa, SIGCONT, SIG_IGN, SA_RESTART);
	SETSIG(sa, SIGCHLD, chld_handler, SA_RESTART);
	
	/* Block sigchild while forking */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, NULL);

	reboot(RB_DISABLE_CAD);

	/*
	 * Parse kernel parameters
	 */
	if ((f = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, LINE_SIZE, f);
		if (strstr(line, "quiet")) {
			close(0);
			close(1);
			close(2);
		}
		fclose(f);
	}

	setsid();

	/*
	 * Mount filesystems
	 */
	mount("proc", "/proc", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devpts", "/dev/pts", "devpts", 0, "gid=5,mode=620");
	mount("tmpfs", "/dev/shm", "tmpfs", 0, NULL);
	mount("tmpfs", "/tmp", "tmpfs", 0, "mode=1777,size=128m");
	mount("tmpfs", "/var/run", "tmpfs", 0, "mode=0755");
	mount("tmpfs", "/var/lock", "tmpfs", 0, "mode=1777");
	mount("/mnt", "/", NULL, MS_MOVE, NULL);

	/*
	 * Time adjustments
	 */
	if ((fd = open("/etc/adjtime", O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
		write(fd, "0.0 0 0.0\n", 10);
		close(fd);
	}
#ifndef NO_HCTOSYS
	system("/sbin/hwclock --hctosys --localtime" HWCLOCK_DIRECTISA);
#endif

	/*
	 * Network stuff
	 */
	makepath("/dev/shm/network");
	makepath("/dev/shm/resolvconf/interface");

	if ((dir = opendir("/etc/resolvconf/run/interface")) != NULL) {
		while ((d = readdir(dir)) != NULL) {
			if (isalnum(d->d_name[0]))
				continue;
			snprintf(line, LINE_SIZE,
				"/etc/resolvconf/run/interface/%s", d->d_name);
			unlink(line);
		}

		closedir(dir);
	}

	touch("/var/run/utmp");
	touch("/etc/resolvconf/run/enable-updates");

	chdir("/etc/resolvconf/run/interface");
#ifdef BUILTIN_RUNPARTS
	run_parts("/etc/resolvconf/update.d", "-i", NULL);
#else
	system("/bin/run-parts --arg=-i /etc/resolvconf/update.d");
#endif
	chdir("/");
	
	touch("/etc/network/run/ifstate");

	if ((f = fopen("/etc/hostname", "r")) != NULL) {
		fgets(line, LINE_SIZE, f);	
		if ((x = strchr(line, 0x0a)) != NULL)
			*x = 0;

		sethostname(line, strlen(line)); 
		fclose(f);
		
	}

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);

	/*
	 * Set random seed
	 */
	copyfile("/var/lib/urandom/random-seed", "/dev/urandom", 0);
	unlink("/var/lib/urandom/random-seed");
	umask(077);
	copyfile("/dev/urandom", "/var/lib/urandom/random-seed", 4096);

	/*
	 * Misc setup
	 */
	unlink("/var/run/.clean");
	unlink("/var/lock/.clean");
	umask(0);
	mkdir("/tmp/.X11-unix", 01777);
	mkdir("/tmp/.ICE-unix", 01777);
	umask(022);

	system("/usr/bin/openvt /sbin/getty 38400 tty2&");

	if (!fork()) {
		/* child process */

		vhangup();

		close(2);
		close(1);
		close(0);

		if (open("/dev/tty1", O_RDWR) != 0)
			exit(1);

		sigemptyset(&act.sa_mask);
		act.sa_handler = SIG_DFL;

		sigemptyset(&nmask2);
		sigaddset(&nmask2, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask2, NULL);

		for (i = 1; i < NSIG; i++)
			sigaction(i, &sa, NULL);

		dup2(0, 0);
		dup2(0, 1);
		dup2(0, 2);

		touch("/tmp/nologin");
		
		while (access("/tmp/shutdown", F_OK) < 0)
			system("su -c startx -l user &> /dev/null");

		exit(0);
	}
	
	/* parent process */

	sleep(1);
	system("/usr/sbin/services.sh &>/dev/null&");

	while (1) {
		sigemptyset(&nmask);
		pselect(0, NULL, NULL, NULL, NULL, &nmask);
	}
}


/*
 * Shut down on INT USR1 USR2
 */
void shutdown(int sig)
{
	touch("/tmp/shutdown");

	kill(-1, SIGTERM);

	write(1, "\033[?25l\033[30;40m", 14);
	copyfile("/boot/shutdown.fb", "/dev/fb/0", 0);
	sleep(2);

	system("/usr/sbin/alsactl store > /dev/null 2>&1");
	system("/sbin/hwclock --systohc --localtime" HWCLOCK_DIRECTISA);

	kill(-1, SIGKILL);

	sync();
	sync();
	system("/bin/mount -n -o remount,ro /");
	system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");

	if (sig == SIGINT || sig == SIGUSR1)
		reboot(RB_AUTOBOOT);

	reboot(RB_POWER_OFF);
}


/*
 * SIGCHLD: one of our children has died
 */
void chld_handler(int sig)
{
	int status;

	while (waitpid(-1, &status, WNOHANG) != 0) {
		if (errno == ECHILD)
			break;
	}
}

