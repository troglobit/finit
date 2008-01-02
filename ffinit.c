
/*
 * Improved fast init
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
#include <linux/limits.h>


/* From sysvinit */
/* Set a signal handler. */
#define SETSIG(sa, sig, fun, flags) \
		do { \
			sa.sa_handler = fun; \
			sa.sa_flags = flags; \
			sigemptyset(&sa.sa_mask); \
			sigaction(sig, &sa, NULL); \
		} while(0)

#define touch(x) close(open((x), O_CREAT|O_WRONLY|O_TRUNC, 0644))


int makepath(char *p)
{
	char *x, path[PATH_MAX];
	int ret;
	
	x = path;

	do {
		do { *x++ = *p++; } while (*p && *p != '/');
		ret = mkdir(path, 0777);
	} while (*p && (*p != '/' || *(p + 1))); /* ignore trailing slash */

	return ret;
}



void shutdown(int);
void signal_handler(int);
void chld_handler(int);


int main()
{
	int i;
	FILE *f;
	char line[2096];
	int fd;
	DIR *dir;
	struct dirent *d;
	char filename[1024];
	struct sigaction sa, act;
	char hline[1024];
	char *x;
	sigset_t nmask, nmask2;

	chdir("/");
	umask(022);
	
	/*
	 * Signal management
	 */
	for (i = 1; i < NSIG; i++)
		SETSIG(sa, i, SIG_IGN, SA_RESTART);

	SETSIG(sa, SIGINT,  shutdown,       0);
	SETSIG(sa, SIGPWR,  signal_handler, 0);
	SETSIG(sa, SIGUSR1, shutdown,       0);
	SETSIG(sa, SIGUSR2, shutdown,       0);
	SETSIG(sa, SIGTERM, signal_handler, 0);
	//SETSIG(sa, SIGKILL, signal_handler, 0);
	SETSIG(sa, SIGALRM, signal_handler, 0);
	SETSIG(sa, SIGHUP,  signal_handler, 0);
	//SETSIG(sa, SIGSTOP, signal_handler, SA_RESTART);
	SETSIG(sa, SIGCONT, signal_handler, SA_RESTART);
	SETSIG(sa, SIGCHLD, chld_handler,   SA_RESTART);
	
	/* Block sigchild while forking */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, NULL);

	reboot(RB_DISABLE_CAD);

	/*
	 * Parse kernel parameters
	 */
	if ((f = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, 2095, f);
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
	mount("/mnt", "/", NULL, MS_MOVE, NULL);

	/*
	 * Time adjustments
	 */
	if ((fd = open("/etc/adjtime", O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
		write(fd, "0.0 0 0.0\n", 10);
		close(fd);
	}
	system("/sbin/hwclock --hctosys --localtime");

	/*
	 * Network stuff
	 */
	makepath("/dev/shm/network");
	makepath("/dev/shm/resolvconf/interface");

	if ((dir = opendir("/etc/resolvconf/run/interface")) != NULL) {
		while ((d = readdir(dir)) != NULL) {
			if (isalnum(d->d_name[0]))
				continue;
			sprintf(filename, "/etc/resolvconf/run/interface/%s",
								d->d_name);
			unlink(filename);
		}

		closedir(dir);
	}

	touch("/var/run/utmp");
	touch("/etc/resolvconf/run/enable-updates");

	chdir("/etc/resolvconf/run/interface");
	system("/bin/run-parts --arg=-i /etc/resolvconf/update.d");
	chdir("/");
	
	touch("/etc/network/run/ifstate");

	if ((f = fopen("/etc/hostname", "r")) != NULL) {
		fgets(hline, 1023, f);	
		if ((x = strchr(hline, 0x0a)) != NULL)
			*x = 0;

		sethostname(hline, strlen(hline)); 
		fclose(f);
		
	}

	system("/sbin/ifconfig lo 127.0.0.1 netmask 255.0.0.0 up > /dev/null");

	/*
	 * Set random seed
	 */
	/* Bug: the eeepc never sets its random seed from file */
	system("/bin/cat /var/lib/urandom/random-seed >/dev/urandom "
						"> /dev/null 2>&1");
	unlink("/var/lib/urandom/random-seed");

	umask(077);
	system("/bin/dd if=/dev/urandom of=/var/lib/urandom/random-seed "
					"bs=4096 count=1 >/dev/null 2>&1");

	/*
	 * Misc setup
	 */
	system("/bin/rm -rf /tmp/* /tmp/.* 2>/dev/null");
	unlink("/var/run/.clean");
	unlink("/var/lock/.clean");
	umask(0);
	mkdir("/tmp/.X11-unix", 01777);
	mkdir("/tmp/.ICE-unix", 01777);
	umask(022);

	if (!fork()) {
		/* child process */

		vhangup();

		close(2);
		close(1);
		close(0);

		if (open("/dev/tty1", O_RDWR, 0))
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

	system("/sbin/getty 38400 tty3 &");
	sleep(1);
	system("/usr/sbin/services.sh &> /dev/null &");

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
	int fd;

	touch("/tmp/shutdown");

	kill(-1, SIGTERM);

	system("/bin/echo -e \"\033[?25l\033[30;40m\"; "
				"/bin/cp /boot/shutdown.b /dev/fb/0");
	sleep(1);
	sleep(1);

	system("/usr/sbin/alsactl store > /dev/null 2>&1");
	system("/sbin/hwclock --systohc --localtime");
	system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");

	kill(-1, SIGKILL);

	sync();
	sync();
	system("/bin/mount -n -o remount,ro /");

	system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");

	if (sig == SIGINT || sig == SIGUSR1)
		reboot(RB_AUTOBOOT);

	if ((fd = open("/proc/acpi/sleep", O_WRONLY)) >= 0) {
		write(fd, "5", 1);
		close(fd);
	}
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


/*
 * We got a signal (PWR TERM ALRM HUP STOP CONT)
 */
void signal_handler(int sig)
{
	/* do nothing */
}

