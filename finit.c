/* Improved fast init
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
 */

#include <string.h>
#include <ctype.h>
#include <sys/mount.h>
#include <dirent.h>
#include <linux/fs.h>
#include <utmp.h>

#include "finit.h"
#include "helpers.h"
#include "service.h"

int   debug   = 0;
char *sdown   = NULL;
char *network = NULL;
char *startx  = NULL;

static char *build_cmd(char *cmd, char *line, int len)
{
	int l;
	char *c;

	/* Trim leading whitespace */
	while (*line && (*line == ' ' || *line == '\t'))
		line++;

	if (!cmd) {
		cmd = malloc (strlen(line) + 1);
		if (!cmd) {
			_e("No memory left for '%s'", line);
			return NULL;
		}
		*cmd = 0;
	}
	c = cmd + strlen(cmd);
	for (l = 0; *line && *line != '#' && *line != '\t' && l < len; l++)
		*c++ = *line++;
	*c = 0;

	_d("cmd = %s", cmd);
	return cmd;
}

int main(int UNUSED(args), char *argv[])
{
	FILE *fp;
	char line[LINE_SIZE];
	char username[USERNAME_SIZE] = DEFUSER;
	char hostname[HOSTNAME_SIZE] = DEFHOST;
	char cmd[CMD_SIZE];
#ifdef USE_ETC_RESOLVCONF_RUN
	DIR *dir;
	struct dirent *d;
#endif
#ifdef RUNLEVEL
	struct utmp entry;
#endif
#ifdef PAM_CONSOLE
	int fd;
#endif

	/*
	 * Initial setup of signals, ignore all until we're up.
	 */
	sig_init();

	/*
	 * Mount base file system, kernel is assumed to run devtmpfs for /dev
	 */
	chdir("/");
	umask(0);
	mount("none", "/proc", "proc", 0, NULL);
	mount("none", "/proc/bus/usb", "usbfs", 0, NULL);
	mount("none", "/sys", "sysfs", 0, NULL);
	mkdir("/dev/pts", 0755);
	mkdir("/dev/shm", 0755);
	mount("none", "/dev/pts", "devpts", 0, "gid=5,mode=620");
	mount("none", "/dev/shm", "tmpfs", 0, NULL);
	umask(022);

	/*
	 * Populate /dev and prepare for runtime events from kernel.
	 */
#if defined(USE_UDEV)
	system("udevd --daemon");
#elif defined (MDEV)
	system(MDEV "-s");
#endif

	/*
	 * Parse kernel parameters
	 */
	if ((fp = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, LINE_SIZE, fp);

		if (strstr(line, "finit_debug") ||
		    strstr(line, "--verbose")   ||
		    strstr(line, "--debug"))
			debug = 1;

		if (!debug && strstr(line, "quiet")) {
			close(0);
			close(1);
			close(2);
		}

		fclose(fp);
	}

	cls();
	echo("finit " VERSION " (built " __DATE__ " " __TIME__ " by " WHOAMI ")");

	/*
	 * Parse configuration file
	 */
	if ((fp = fopen(FINIT_CONF, "r")) != NULL) {
		char *x;

		_d("Parse %s ...", FINIT_CONF);
		while (!feof(fp)) {
			if (!fgets(line, LINE_SIZE, fp))
				continue;
			chomp(line);

			_d("conf: %s", line);

			/* Skip comments. */
			if (MATCH_CMD(line, "#", x)) {
				continue;
			}
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
			if (MATCH_CMD(line, "host ", x)) {
				*hostname = 0;
				build_cmd(hostname, x, HOSTNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "shutdown ", x)) {
				if (sdown) free(sdown);
				sdown = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "module ", x)) {
				strcpy(cmd, "/sbin/modprobe ");
				build_cmd(cmd, x, CMD_SIZE);
				system(cmd);
				continue;
			}
			if (MATCH_CMD(line, "mknod ", x)) {
				strcpy(cmd, "/bin/mknod ");
				build_cmd(cmd, x, CMD_SIZE);
				system(cmd);
				continue;
			}
			if (MATCH_CMD(line, "network ", x)) {
				if (network) free(network);
				network = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "startx ", x)) {
				if (startx) free(startx);
				startx = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "service ", x)) {
				if (service_register (x))
					_e("Failed, too many services to monitor.\n");
				continue;
			}
		}
		fclose(fp);
	}

	/*
	 * Mount filesystems
	 */
	_d("Mount filesystems in /etc/fstab ...");

#ifdef REMOUNT_ROOTFS_RW
	system("/bin/mount -n -o remount,rw /");
#endif
#ifdef SYSROOT
	mount(SYSROOT, "/", NULL, MS_MOVE, NULL);
#endif
	umask(0);
	system("mount -na");
	system("swapon -ea");
	umask(0022);

	/* Cleanup stale files, if any still linger on. */
	system("rm -rf /tmp/* /var/run/* /var/lock/*");

	/*
	 * Base FS up, enable standard SysV init signals
	 */
	sig_setup();

	/*
	 * Time adjustments
	 */
#ifndef NO_HCTOSYS
	_d("Adjust system clock ...");
	system("/sbin/hwclock --hctosys");
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

#ifdef RUNLEVEL
	touch("/var/run/utmp");
	chown("/var/run/utmp", 0, getgroup("utmp"));
	chmod("/var/run/utmp", 0664);

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
#endif /* BUILTIN_RUNPARTS */
	chdir("/");
#endif /* USE_ETC_RESOLVCONF_RUN */

#ifdef TOUCH_ETC_NETWORK_RUN_IFSTATE
	touch("/etc/network/run/ifstate");
#else
	remove("/etc/network/run/ifstate");
#endif

	/* Set initial hostname. */
	set_hostname(hostname);

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);
	if (network) {
		_d("Starting networking: %s", network);
		system(network);
	}

	/*
	 * Set random seed
	 */
#ifdef RANDOMSEED
	copyfile(RANDOMSEED, "/dev/urandom", 0);
	unlink(RANDOMSEED);
	umask(077);
	copyfile("/dev/urandom", RANDOMSEED, 4096);
	umask(0);
#endif

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
	mkdir("/var/run/sshd", 01755);
	umask(022);

#ifdef USE_MESSAGE_BUS
	_d("Starting D-Bus ...");
	mkdir("/var/run/dbus", 0755);
	mkdir("/var/lock/subsys/messagebus", 0755);
	system("dbus-uuidgen --ensure");
	system("su -c \"dbus-daemon --system\" messagebus");
#endif

	_d("Starting services from %s", FINIT_CONF);
	service_startup();

#ifdef LISTEN_INITCTL
	listen_initctl();
#endif

	if (!fork()) {
		/* child process */
		int i;
		char c;
		sigset_t nmask;
		struct sigaction sa;

		vhangup();

		close(2);
		close(1);
		close(0);

		if (open(CONSOLE, O_RDWR) != 0)
			exit(1);

		sigemptyset(&sa.sa_mask);
		sa.sa_handler = SIG_DFL;

		sigemptyset(&nmask);
		sigaddset(&nmask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask, NULL);

		for (i = 1; i < NSIG; i++)
			sigaction(i, &sa, NULL);

		dup2(0, STDIN_FILENO);
		dup2(0, STDOUT_FILENO);
		dup2(0, STDERR_FILENO);

		set_procname(argv, "console");

		/* ConsoleKit needs this */
		setenv("DISPLAY", ":0", 1);

		/* Run startup scripts in /etc/finit.d/, if any. */
		run_parts(FINIT_RCSD);

		while (!fexist(SYNC_SHUTDOWN)) {
			if (fexist(SYNC_STOPPED)) {
				sleep(1);
				continue;
			}

			if (startx && !debug) {
				snprintf(line, sizeof(line), "su -c \"%s\" -l %s", startx, username);
				system(line);
			} else {
				static const char msg[] = "\nPlease press Enter to activate this console. ";

				i = write(STDERR_FILENO, msg, sizeof(msg) - 1);
				while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n')
				continue;

				if (fexist(SYNC_STOPPED))
					continue;

				system(GETTY);
			}
		}

		exit(0);
	}

	service_monitor();

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
