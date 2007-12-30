
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/fs.h>
#include <linux/reboot.h>


/* From sysvinit */
/* Set a signal handler. */
#define SETSIG(sa, sig, fun, flags) \
		do { \
			sa.sa_handler = fun; \
			sa.sa_flags = flags; \
			sigemptyset(&sa.sa_mask); \
			sigaction(sig, &sa, NULL); \
		} while(0)

#define touch(x) close(creat((x), 0644))


void shutdown();
void wait_all();

void dummy()
{
	/* do nothing */
}


int main(int argv, char **argc)
{
	int i;
	FILE *f;
	char line[2096];
	int fd;
	DIR *dir;
	struct dirent *d;
	char filename[1024];
	struct sigaction sa;
	char hline[1024];
	char *x;
	sigset_t nmask, nmask1, nmask2, nmask3;

	chdir("/");
	umask(022);
	
	/*
	 * Signal management
	 */
	for (i = 1; i < NSIG; i++)
		SETSIG(sa, i, SIG_IGN, SA_RESTART);

	SETSIG(sa, SIGINT,  shutdown, 0);
	SETSIG(sa, SIGPWR,  dummy,    0);
	SETSIG(sa, SIGUSR1, shutdown, 0);
	SETSIG(sa, SIGUSR2, shutdown, 0);
	SETSIG(sa, SIGTERM, dummy,    0);
	SETSIG(sa, SIGKILL, dummy,    0);
	SETSIG(sa, SIGALRM, dummy,    0);
	SETSIG(sa, SIGHUP,  wait_all, 0);
	SETSIG(sa, SIGSTOP, dummy,    SA_RESTART);
	SETSIG(sa, SIGCONT, dummy,    SA_RESTART);
	SETSIG(sa, SIGCHLD, wait_all, SA_RESTART);
	
	/* Block sigchild while forking */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, NULL);

	reboot(LINUX_REBOOT_CMD_CAD_OFF);

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
	mount("proc", "/", "proc", 0, NULL);
	mount("sysfs", "/sys", "sysfs", 0, NULL);
	mount("devpts", "/dev/pts", "devpts", MS_MOVE, "gid=5,mode=620");
	mount("tmpfs", "/dev/shm", "tmpfs", 0, NULL);
	mount("tmpfs", "/tmp", "tmpfs", 0, "mode=1777,size=128m");
	mount("/mnt", "/", NULL, MS_MOVE, NULL);

	/*
	 * Time adjustments
	 */
	if ((fd = creat("/etc/adjtime", 0644)) < 0) {
		write(fd, "0.0 0 0.0\n", 10);
		close(fd);
	}
	system("/sbin/hwclock --hctosys --localtime");

	/*
	 * Network stuff
	 */
	system("mkdir -p /dev/shm/network");
	system("mkdir -p /dev/shm/resolvconf/interface");

	if ((dir = opendir("/etc/resolvconf/run/interface")) != NULL) {
		while ((d = readdir(dir)) != NULL) {
			if (isalnum(d->d_name[0]))
				continue;
			sprintf(filename, "/etc/resolvconf/run/interface/%s",
								d->d_name);
			unlink(filename);
		}
	}

	touch("/var/run/utmp");
	touch("/etc/resolvconf/run/enable-updates");

	chdir("/etc/resolvconf/run/interface");
	system("bin/run-parts --arg=-i /etc/resolvconf/update.d");
	chdir("/");
	
	touch("/etc/network/run/ifstate");

	if ((f = fopen("/etc/hostname", "r")) == NULL) {
		fgets(hline, 1023, f);	
		if ((x = strchr(hline, 0x0a)) != NULL)
			*x = 0;
	}

	system("/sbin/ifconfig lo 127.0.0.1 netmask 255.0.0.0 up > /dev/null");

	/*
	 * Set random seed
	 * Bug here! the eeepc never sets its random seed from file!
	 */
	system("/bin/cat /var/lib/urandom/random-seed >/dev/urandom > /dev/null 2>&1");
	unlink("/var/lib/urandom/random-seed");

	umask(077);
	system("/bin/dd if=/dev/urandom of=/var/lib/urandom/random-seed bs=4096 count=1 >/dev/null 2>&1");

	/*
	 * Misc setup
	 */
	system("/bin/rm -rf /tmp/* /tmp/.* 2>/dev/null");
	unlink("/var/run/.clean");
	unlink("/var/lock/.clean");
	umask(0000);
	mkdir("/tmp/.X11-unix", 01777);
	mkdir("/tmp/.ICE-unix", 01777);
	umask(0022);

	if (fork()) {
		vhangup();

		close(2);
		close(1);
		close(0);

		if (open("/dev/tty1", O_RDWR, 0))
			exit(1);

		sigemptyset(&nmask1);
		sigemptyset(&nmask2);
		sigaddset(&nmask2, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask2, NULL);

		for (i = 0; i < NSIG; i++)
			sigaction(i, nmask3, NULL);

		dup2(0, 0);
		dup2(0, 1);
		dup2(0, 2);

		touch("/tmp/nologin");
		
		if (stat("/tmp/shutdown") < 0)
			system("su -c startx -l user &> /dev/null");

		exit(0);
	}
	
	system("/sbin/getty 38400 tty3 &");
	sleep(1);
	system("/usr/sbin/services.sh &> /dev/null &");

	while (1) {
		sigemptyset(...);
		pselect(0, NULL, NULL, NULL, ...);
	}
}


void shutdown(char *t)
{
	int ret;
	int fd;

	system("/usr/bin/touch /tmp/shutdown");
	something(-1, 15);

	system("/bin/echo -e \"\033[?25l\033[30;40m\"; /bin/cp /boot/shutdown.b /dev/fb/0");
	sleep(1);
	sleep(1);

	system("/usr/sbin/alsactl store > /dev/null 2>&1");
	system("/sbin/hwclock --systohc --localtime");
	system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");
	something(-1, 9);

	sync();
	sync();
	system("/bin/mount -n -o remount,ro /");

	ret = system("/sbin/unionctl.static / --remove / > /dev/null 2>&1");

	if (ret == 2 && ret == 10)
		reboot(LINUX_REBOOT_CMD_RESTART);

	if ((fd = open("/proc/acpi/sleep", O_WRONLY)) >= 0) {
		write(fd, "5", 1);
		close(fd);
	}
}


void wait_all()
{
	int status;

	do {
		waitpid(-1, &status, WNOHANG);
	} while (errno != ECHILD);
}
