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
#include <utmp.h>

#include "helpers.h"

#ifndef DEFUSER
#define DEFUSER "user"
#endif

/* Distribution configuration */

#if defined DIST_MDV	/* Mandriva */
#define RANDOMSEED	"/var/lib/random-seed"
#define SYSROOT		"/sysroot"
#define GETTY		"/usr/bin/openvt /sbin/mingetty tty2"
#define RUNPARTS	"/usr/bin/run-parts"
#define REMOUNT_ROOTFS_RW
#define MAKE_DEVICES
#define MAKE_DEV_VCS
#define PAM_CONSOLE
#define LISTEN_INITCTL
#define RUNLEVEL	5
#define USE_MESSAGE_BUS
#elif defined DIST_EEEXUBUNTU	/* eeeXubuntu */
#define RANDOMSEED	"/var/lib/urandom/random-seed"
#define SYSROOT		"/sysroot"
#define GETTY		"/usr/bin/openvt /sbin/getty 38400 tty2"
#define RUNPARTS	"/bin/run-parts"
#define REMOUNT_ROOTFS_RW
#define MAKE_DEVICES
#define TOUCH_ETC_NETWORK_RUN_IFSTATE
#define LISTEN_INITCTL
#else			/* original Eeepc distribution */
#define RANDOMSEED	"/var/lib/urandom/random-seed"
#define SYSROOT		"/mnt"
#define GETTY		"/usr/bin/openvt /sbin/getty 38400 tty2"
#define RUNPARTS	"/bin/run-parts"
#define TOUCH_ETC_NETWORK_RUN_IFSTATE
#define USE_ETC_RESOLVCONF_RUN
#endif

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
		} while (0)

#ifdef LISTEN_INITCTL

#define INIT_MAGIC		0x03091969
#define INIT_CMD_RUNLVL		1

struct init_request {
	int	magic;			/* Magic number                 */
	int	cmd;			/* What kind of request         */
	int	runlevel;		/* Runlevel to change to        */
	int	sleeptime;		/* Time between TERM and KILL   */
	char	data[368];
};
#endif

#define touch(x) mknod((x), S_IFREG|0644, 0)
#define chardev(x,m,maj,min) mknod((x), S_IFCHR|(m), makedev((maj),(min)))
#define blkdev(x,m,maj,min) mknod((x), S_IFBLK|(m), makedev((maj),(min)))

#define MATCH_CMD(l,c,x) \
	((!strncmp((l), (c), strlen((c)))) && ((x) = (l) + strlen((c))))

#define _d(x...) do { if (debug) { printf(x); printf("\n"); } } while (0)

#define LINE_SIZE 1024
#define CMD_SIZE 256
#define USERNAME_SIZE 16
#define HOSTNAME_SIZE 32

void shutdown(int);
void signal_handler(int);
void chld_handler(int);

static int debug = 0;


static void build_cmd(char *cmd, char *x, int len)
{
	int l;
	char *c;

	c = cmd + strlen(cmd);

	/* skip spaces */
	for (; *x && (*x == ' ' || *x == '\t'); x++); 

	/* copy next arg */
	for (l = 0; *x && *x != '#' && *x != '\t' && l < len; l++)
		*c++ = *x++;
	*c = 0;

	_d("cmd = %s", cmd);
}


#ifdef LISTEN_INITCTL

/* Standard reboot/shutdown utilities talk to init using /dev/initctl.
 * We should check if the fifo was recreated and reopen it.
 */
static void listen_initctl()
{
	if (!fork()) {	
		int ctl;	
		fd_set fds;	
		struct init_request request;

		mkfifo("/dev/initctl", 0600);
		ctl = open("/dev/initctl", O_RDONLY);

		while (1) {
			FD_ZERO(&fds);
			FD_SET(ctl, &fds);
			if (select(ctl + 1, &fds, NULL, NULL, NULL) <= 0)
				continue;

			read(ctl, &request, sizeof(request));

			if (request.magic != INIT_MAGIC)
				continue;

			if (request.cmd == INIT_CMD_RUNLVL) {
				switch (request.runlevel) {
				case '0':
					shutdown(SIGUSR2);
					break;
				case '6':
					shutdown(SIGUSR1);
				}
			}
		}
	}
}
#endif


int main()
{
	int i;
	FILE *f;
	char line[LINE_SIZE];
	int fd;
	struct sigaction sa, act;
	sigset_t nmask, nmask2;
	char username[USERNAME_SIZE] = DEFUSER;
	char hostname[HOSTNAME_SIZE] = "eviltwin";
	char cmd[CMD_SIZE];
	char startx[CMD_SIZE] = "xinit";
#ifdef USE_ETC_RESOLVCONF_RUN
	DIR *dir;
	struct dirent *d;
#endif
#ifdef RUNLEVEL
	struct utmp entry;
#endif

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

	mount("none", "/proc", "proc", 0, NULL);

	/*
	 * Parse kernel parameters
	 */
	if ((f = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, LINE_SIZE, f);
		if ((strstr(line, "finit_debug"))) {
			debug = 1;
		}
		if (strstr(line, "quiet")) {
			close(0);
			close(1);
			close(2);
		}
		fclose(f);
	}

	_d("finit-alt " VERSION " (built " __DATE__ " " __TIME__ " by " WHOAMI ")");

	setsid();

	/*
	 * Parse configuration file
	 */
	if ((f = fopen("/etc/finit.conf", "r")) != NULL) {
		char *x;
		while (!feof(f)) {
			fgets(line, LINE_SIZE, f);
			chomp(line);

			_d("conf: %s", line);

			/* Do this before mounting / read-write */
			if (MATCH_CMD(line, "check ", x)) {
				strcpy(cmd, "/sbin/fsck -C -a ");
				build_cmd(cmd, x, CMD_SIZE);
				system(cmd);
				continue;
			}
			if (MATCH_CMD(line, "user ", x)) {
				*username = 0;
				build_cmd(username, x, USERNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "startx ", x)) {
				*startx = 0;
				build_cmd(startx, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "host ", x)) {
				*hostname = 0;
				build_cmd(hostname, x, HOSTNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "module ", x)) {
				strcpy(cmd, "/sbin/modprobe ");
				build_cmd(cmd, x, CMD_SIZE);
				system(cmd);
				continue;
			}
#ifdef MAKE_DEVICES
			/* This is used if the system uses udev and doesn't
			 * mount /dev as tmpfs in the initrd, or if it doesn't
			 * use an initrd. Mount /dev as tmpfs and create basic
			 * device nodes.
			 */
			if (MATCH_CMD(line, "mountdev", x)) {
				mount("none", "/dev", "tmpfs", 0, "mode=0755");
				chardev("/dev/null", 0666, 1, 3);
				blkdev("/dev/sda1", 0660, 8, 1);
				blkdev("/dev/sda2", 0660, 8, 2);
				blkdev("/dev/sda3", 0660, 8, 3);
				blkdev("/dev/sda4", 0660, 8, 4);
				blkdev("/dev/sda5", 0660, 8, 5);
				blkdev("/dev/sda6", 0660, 8, 6);
#ifdef MAKE_DEV_VCS
			/* Caio reports that newer X.org needs these */
				blkdev("/dev/vcs1", 0660, 7, 1);
				blkdev("/dev/vcs2", 0660, 7, 2);
				blkdev("/dev/vcs3", 0660, 7, 3);
				blkdev("/dev/vcs4", 0660, 7, 4);
				blkdev("/dev/vcsa1", 0660, 7, 129);
				blkdev("/dev/vcsa2", 0660, 7, 130);
				blkdev("/dev/vcsa3", 0660, 7, 131);
				blkdev("/dev/vcsa4", 0660, 7, 132);
#endif
				continue;
			}
			/* This works only if /dev is tmpfs! If not, create
			 * devices statically on the filesystem
			 */
			if (MATCH_CMD(line, "mknod ", x)) {
				strcpy(cmd, "/bin/mknod ");
				build_cmd(cmd, x, CMD_SIZE);
				system(cmd);
				continue;
			}
#endif
		}
		fclose(f);
	}

	/*
	 * Mount filesystems
	 */
	_d("mount filesystems");

#ifdef REMOUNT_ROOTFS_RW
	system("/bin/mount -n -o remount,rw /");
#endif
	umask(0);

#ifdef MAKE_DEVICES
	mkdir("/dev/shm", 0755);
	mkdir("/dev/pts", 0755);
#endif

	mount("none", "/sys", "sysfs", 0, NULL);
	mount("none", "/dev/pts", "devpts", 0, "gid=5,mode=620");
	mount("none", "/dev/shm", "tmpfs", 0, NULL);
	mount("none", "/tmp", "tmpfs", 0, "mode=1777,size=128m");
	mount("none", "/var/run", "tmpfs", 0, "mode=0755");
	mount("none", "/var/lock", "tmpfs", 0, "mode=1777");
	mount("none", "/proc/bus/usb", "usbfs", 0, NULL);
	mount(SYSROOT, "/", NULL, MS_MOVE, NULL);

	_d("make devices");
#ifdef MAKE_DEVICES
	mkdir("/dev/input", 0755);
	chardev("/dev/urandom", 0666, 1, 9);
	chardev("/dev/ptmx", 0666, 5, 2);
	chardev("/dev/null", 0666, 1, 3);
	chmod("/dev/null", 0666);
	chardev("/dev/mem",  0640, 1, 1);
	chardev("/dev/tty0",  0660, 4, 0);
	chardev("/dev/tty1",  0660, 4, 1);
	chardev("/dev/tty2",  0660, 4, 2);
	chardev("/dev/tty3",  0660, 4, 3);
	chardev("/dev/input/mice",  0660, 13, 63);
	chardev("/dev/input/event0",  0660, 13, 64);
	chardev("/dev/agpgart",  0660, 10, 175);
	blkdev("/dev/loop0",  0600, 7, 0);
#endif

	unlink("/etc/mtab");
	system("mount -a");

	umask(0022);

	/*
	 * Time adjustments
	 */
	_d("adjust clock");
#ifndef NO_HCTOSYS
	system("/sbin/hwclock --hctosys" HWCLOCK_DIRECTISA);
#endif

	/*
	 * Network stuff
	 */

#ifdef USE_ETC_RESOLVCONF_RUN
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
#endif

#ifdef USE_VAR_RUN_RESOLVCONF
	makepath("/var/run/resolvconf/interface");
	symlink("../../../etc/resolv.conf", "/var/run/resolvconf/resolv.conf");
#endif

	touch("/var/run/utmp");
	chown("/var/run/utmp", 0, getgroup("utmp"));
	chmod("/var/run/utmp", 0664);

#ifdef RUNLEVEL
	memset(&entry, 0, sizeof(struct utmp));
	entry.ut_type = RUN_LVL;
	entry.ut_pid = '0' + RUNLEVEL;
	setutent();
	pututline(&entry);
	endutent();
#endif

#ifdef USE_ETC_RESOLVCONF_RUN
	touch("/etc/resolvconf/run/enable-updates");

	chdir("/etc/resolvconf/run/interface");
#ifdef BUILTIN_RUNPARTS
	run_parts("/etc/resolvconf/update.d", "-i", NULL);
#else
	system(RUNPARTS " --arg=i /etc/resolvconf/update.d");
#endif
	chdir("/");
#endif
	
#ifdef TOUCH_ETC_NETWORK_RUN_IFSTATE
	touch("/etc/network/run/ifstate");
#endif

	if ((f = fopen("/etc/hostname", "r")) != NULL) {
		fgets(hostname, HOSTNAME_SIZE, f);	
		chomp(hostname);
		fclose(f);
	}

	sethostname(hostname, strlen(hostname)); 

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);

	/*
	 * Set random seed
	 */
	copyfile(RANDOMSEED, "/dev/urandom", 0);
	unlink(RANDOMSEED);
	umask(077);
	copyfile("/dev/urandom", RANDOMSEED, 4096);
	umask(0);

	/*
	 * Console setup (for X)
	 */
	makepath("/var/run/console");
	snprintf(line, LINE_SIZE, "/var/run/console/%s", username);
	touch(line);

#ifdef PAM_CONSOLE
	if ((fd = open("/var/run/console/console.lock", O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
		write(fd, username, strlen(username));
		close(fd);
		system("/sbin/pam_console_apply");
	}
#endif

	/*
	 * Misc setup
	 */
	mkdir("/tmp/.X11-unix", 01777);
	mkdir("/tmp/.ICE-unix", 01777);
	umask(022);

	system(GETTY "&");

	_d("forking");
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

#ifdef USE_MESSAGE_BUS
		_d("dbus");
		mkdir("/var/run/dbus", 0755);
		mkdir("/var/lock/subsys/messagebus", 0755);
		system("dbus-uuidgen --ensure;dbus-daemon --system");
#endif

#ifdef LISTEN_INITCTL
		listen_initctl();
#endif

		/* Prevents bash from running loadkeys */
		unsetenv("TERM");

		/* ConsoleKit needs this */
		setenv("DISPLAY", ":0", 1);

		while (access("/tmp/shutdown", F_OK) < 0) {
			_d("start X as %s\n", username);
			if (debug) {
				snprintf(line, LINE_SIZE,
					"su -l %s -c \"%s\"", username, startx);
				system(line);
				system("/bin/sh");
			} else {
				snprintf(line, LINE_SIZE,
					"su -l %s -c \"%s\" &> /dev/null",
					username, startx);
				system(line);
			}
		}

		exit(0);
	}
	
	/* parent process */

	
	/* sleep(1); */
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
	system("/sbin/hwclock --systohc" HWCLOCK_DIRECTISA);

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

