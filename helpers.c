/* Misc. utility functions and C-library extensions for finit and its plugins
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

#include <ctype.h>		/* isdigit() */
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <dirent.h>
#include <stdarg.h>
#include <sys/wait.h>

#include <grp.h>
#include <pwd.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "sig.h"
#include "lite.h"

#define NUM_ARGS    16
#define NUM_SCRIPTS 128		/* ought to be enough for anyone */

/*
 * Helpers to replace system() calls
 */

int makepath(char *path)
{
	char buf[PATH_MAX];
	char *x = buf;
	int ret;

	if (!path) {
		errno = EINVAL;
		return -1;
	}

	do {
		do {
			*x++ = *path++;
		} while (*path && *path != '/');

		*x = 0;
		ret = mkdir(buf, 0777);
	} while (*path && (*path != '/' || *(path + 1))); /* ignore trailing slash */

	return ret;
}

void ifconfig(char *ifname, char *addr, char *mask, int up)
{
	struct ifreq ifr;
	struct sockaddr_in *a = (struct sockaddr_in *)&ifr.ifr_addr;
	int sock;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		return;

	memset(&ifr, 0, sizeof (ifr));
	strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
	ifr.ifr_addr.sa_family = AF_INET;

	if (up) {
		if (addr) {
			inet_aton(addr, &a->sin_addr);
			ioctl(sock, SIOCSIFADDR, &ifr);
		}
		if (mask) {
			inet_aton(mask, &a->sin_addr);
			ioctl(sock, SIOCSIFNETMASK, &ifr);
		}
	}

	ioctl(sock, SIOCGIFFLAGS, &ifr);

	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	ioctl(sock, SIOCSIFFLAGS, &ifr);
	
	close(sock);
}


/**
 * pidfile_read - Reads a PID value from a pidfile.
 * @pidfile: File containing PID, usually in /var/run/<PROC>.pid
 *
 * This function takes a @pidfile and returns the PID found therein.
 *
 * Returns:
 * On invalid @pidfile -1 and @errno set to %EINVAL, when @pidfile does not exist -1
 * and errno set to %ENOENT.  When the pidfile is empty or when its contents cannot
 * be translated this function returns zero (0), on success this function returns
 * a PID value greater than one. PID 1 is reserved for the system init process.
 */
pid_t pidfile_read(char *pidfile)
{
   pid_t pid = 0;
   char buf[16];
   FILE *fp;

   if (!pidfile) {
      errno = EINVAL;
      return -1;
   }

   if (!fexist(pidfile))
      return -1;

   fp = fopen(pidfile, "r");
   if (!fp)
      return -1;

   if (fgets(buf, sizeof(buf), fp)) {
      errno = 0;
      pid = strtoul(buf, NULL, 0);
      if (errno)
         pid = 0;               /* Failed conversion. */
   }
   fclose(fp);

   return pid;
}

/**
 * pidfile_poll - Poll for the existence of a pidfile and return PID
 * @cmd:  Process name, or command, called to expect a pidfile
 * @path: Path to pidfile to poll for
 *
 * This function polls for the pidfile at @path for at most 5 seconds
 * before timing out. If the file is created within that time span the
 * file is read and its PID contents returned.
 *
 * Returns:
 * The PID read from @path, or zero on timeout.
 */
pid_t pidfile_poll(char *cmd, char *path)
{
	pid_t pid = 0;
	int tries = 0;

	/* Timeout = 100 * 50ms = 5s */
	while (!fexist(path) && tries++ < 100) {
		/* Wait 50ms between retries */
		usleep(50000);
	}

	if (!fexist(path)) {
		_e("Timeout! No PID found for %s, pidfile %s does not exist?", cmd, path);
		pid = 0;
	} else {
		pid = pidfile_read(path);
	}

	return pid;
}


/**
 * pid_alive - Check if a given process ID is running
 * @pid: Process ID to check for.
 *
 * Returns:
 * %TRUE(1) if pid is alive (/proc/pid exists), otherwise %FALSE(0)
 */
int pid_alive(pid_t pid)
{
	char name[24]; /* Enough for max pid_t */

	snprintf(name, sizeof(name), "/proc/%d", pid);

	return fexist(name);
}


/**
 * pid_get_name - Find name of a process
 * @pid:  PID of process to find.
 * @name: Pointer to buffer where to return process name, may be %NULL
 * @len:  Length in bytes of @name buffer.
 *
 * If @name is %NULL, or @len is zero the function will return
 * a static string.  This may be useful to one-off calls.
 *
 * Returns:
 * %NULL on error, otherwise a va
 */
char *pid_get_name(pid_t pid, char *name, size_t len)
{
	int ret = 1;
	FILE *fp;
	char path[32];
	static char line[64];

	if (!name || len == 0)
		name = line;

	snprintf(path, sizeof(path), "/proc/%d/status", pid);
	if ((fp = fopen(path, "r")) != NULL) {
		if (fgets(line, sizeof (line), fp)) {
			char *pname = line + 6; /* Skip first part of line --> "Name:\t" */

			chomp(pname);
			strlcpy(name, pname, len);
			ret = 0;		 /* Found it! */
		}

		fclose(fp);
	}

	if (ret)
		return NULL;

	return name;
}

/**
 * procname_kill - Send a signal to a process group by name.
 * @name: Name of process to send signal to.
 * @signo: Signal to send.
 *
 * Send a signal to a running process (group). This function searches
 * for a given process @name and sends a signal, @signo, to each
 * process ID matching that @name.
 *
 * Returns:
 * Number of signals sent, i.e. number of processes who have received the signal.
 */
int procname_kill (char *name, int signo)
{
	int result = 0;
	char path[32], line[64];
	FILE *fp;
	DIR *dir;
	struct dirent *entry;

	dir = opendir("/proc");
	if (!dir || !name) {
		errno = EINVAL;
		return 0;
	}

	while ((entry = readdir (dir)) != NULL) {
		/* Skip non-process entries in /proc */
		if (!isdigit (*entry->d_name))
			continue;

		snprintf (path, sizeof(path), "/proc/%s/status", entry->d_name);
		/* Skip non-readable files (protected?) */
		if ((fp = fopen (path, "r")) == NULL)
			continue;

		if (fgets (line, sizeof (line), fp)) {
			char *pname = line + 6; /* Skip first part of line --> "Name:\t" */

			if (strncmp(pname, name, strlen(pname) - 1) == 0) {
				int error = errno;

				if (kill(atoi(entry->d_name), signo)) {
					FLOG_ERROR("Failed signalling(%d) %s: %s!", signo, name, strerror(error));
				} else {
					result++; /* Track number of processes we deliver the signal to. */
				}
			}
		}

		fclose(fp);
	}

	closedir(dir);

	return result;
}


static int print_uptime(void)
{
#if defined(CONFIG_PRINTK_TIME)
	FILE * uptimefile;
	float num1, num2;
	char uptime_str[30];

	if((uptimefile = fopen("/proc/uptime", "r")) == NULL)
		return 1;

	fgets(uptime_str, 20, uptimefile);
	fclose(uptimefile);

	sscanf(uptime_str, "%f %f", &num1, &num2);
	sprintf(uptime_str, "[ %.6f ]", num1);

	write(STDERR_FILENO, uptime_str, strlen(uptime_str));
#endif
	return 0;
}

void print_desc(char *action, char *desc)
{
	const char home[] = "\r\e[K";
	const char dots[] = " .....................................................................";

	write(STDERR_FILENO, home, strlen(home));

	print_uptime();

	write(STDERR_FILENO, action, strlen(action));
	write(STDERR_FILENO, desc, strlen(desc));
	write(STDERR_FILENO, dots, 60 - strlen(desc) - strlen(action)); /* pad with dots. */
}

int print_result(int fail)
{
	if (fail)
		fprintf(stderr, " \e[7m[FAIL]\e[0m\n");
	else
		fprintf(stderr, " \e[1m[ OK ]\e[0m\n");

	return fail;
}

int run(char *cmd)
{
	int status, result, i = 0;
	FILE *fp;
	char *args[NUM_ARGS + 1], *arg, *backup;
	pid_t pid;

	/* We must create a copy that is possible to modify. */
	backup = arg = strdup(cmd);
	if (!arg)
		return 1; /* Failed allocating a string to be modified. */

	/* Split command line into tokens of an argv[] array. */
	args[i++] = strsep(&arg, "\t ");
	do {
		/* Handle run("su -c \"dbus-daemon --system\" messagebus");
		 *   => "su", "-c", "\"dbus-daemon --system\"", "messagebus" */
		if (*arg == '\'' || *arg == '"') {
			char *p, delim[2] = " ";

			delim[0]  = arg[0];
			args[i++] = arg++;
			strsep(&arg, delim);
			 p     = arg - 1;
			*p     = *delim;
			*arg++ = 0;
		} else {
			args[i++] = strsep(&arg, "\t ");
		}
	} while (arg && i < NUM_ARGS);
	args[i] = NULL;
#if 0
	_e("Splitting: '%s' =>", cmd);
	for (i = 0; args[i]; i++)
		_e("\t%s", args[i]);
#endif
	if (i == NUM_ARGS && args[i]) {
		_e("Command too long: %s", cmd);
		free(backup);
		errno = EOVERFLOW;
		return 1;
	}

	fp = fopen("/dev/null", "w");
	pid = fork();
	if (0 == pid) {
		int i;
		struct sigaction sa;

		/* Reset signal handlers that were set by the parent process */
                for (i = 1; i < NSIG; i++)
			DFLSIG(sa, i, 0);

		/* Redirect stdio if the caller requested so. */
		if (fp) {
			int fd = fileno(fp);

			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
		}

		execvp(args[0], args);
		_exit(1); /* Only if execv() fails. */
	} else if (-1 == pid) {
		_pe("%s", args[0]);
		return -1;
	}

	if (waitpid(pid, &status, 0) == -1) {
		if (errno == EINTR)
			_e("Caught unblocked signal waiting for %s, aborting.", args[0]);
		else if (errno == ECHILD)
			_e("Caught SIGCHLD waiting for %s, aborting.", args[0]);
		else
			_e("Failed starting %s, error %d: %s", args[0], errno, strerror (errno));

		if (fp) fclose(fp);
		free(backup);

		return 1;
	}

	result = WEXITSTATUS(status);
	if (WIFEXITED(status)) {
		_d("Started %s and ended OK: %d", args[0], result);
	} else if (WIFSIGNALED(status)) {
		_d("Process %s terminated by signal %d", args[0], WTERMSIG(status));
		if (!result)
			result = 1; /* Must alert callee that the command did complete successfully.
				     * This is necessary since not all programs trap signals and
				     * change their return code accordingly. --Jocke */
	}

	if (fp) fclose(fp);
	free(backup);

	return result;
}

int run_interactive(char *cmd, char *fmt, ...)
{
	int status, oldout = 1, olderr = 2;
	char line[LINE_SIZE];
	va_list ap;
	FILE *fp = tmpfile();

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);

	print_desc("", line);
	if (fp && !debug) {
		oldout = dup(STDOUT_FILENO);
		olderr = dup(STDERR_FILENO);
		dup2(fileno(fp), STDOUT_FILENO);
		dup2(fileno(fp), STDERR_FILENO);
	}
	status = run(cmd);
	if (fp && !debug) {
		dup2(oldout, STDOUT_FILENO);
		dup2(olderr, STDERR_FILENO);
	}
	print_result(status);
	if (fp && !debug) {
		size_t len, written;

		rewind(fp);
		do {
			len     = fread(line, 1, sizeof(line), fp);
			written = fwrite(line, len, sizeof(char), stderr);
		} while (len > 0 && written == len);
		fclose(fp);
	}

	return status;
}

pid_t run_getty(char *cmd, int console)
{
	pid_t pid = fork();

	if (!pid) {
		int i;
		char c;
		sigset_t nmask;
		struct sigaction sa;

		if (console) {
			/* Detach from initial controlling TTY */
			vhangup();

			close(2);
			close(1);
			close(0);

			/* Attach TTY to console */
			if (open(CONSOLE, O_RDWR) != 0)
				exit(1);

			dup2(0, STDIN_FILENO);
			dup2(0, STDOUT_FILENO);
			dup2(0, STDERR_FILENO);

			prctl(PR_SET_NAME, "console", 0, 0, 0);
		}
		sigemptyset(&nmask);
		sigaddset(&nmask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask, NULL);

		/* Reset signal handlers that were set by the parent process */
		for (i = 1; i < NSIG; i++)
			DFLSIG(sa, i, 0);

		while (!fexist(SYNC_SHUTDOWN)) {
			static const char msg[] = "\nPlease press Enter to activate this console. ";

			if (fexist(SYNC_STOPPED)) {
				sleep(1);
				continue;
			}

			if (console) {
				i = write(STDERR_FILENO, msg, sizeof(msg) - 1);
				while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n')
					continue;
			}
			if (fexist(SYNC_STOPPED))
				continue;

			run(cmd);
		}

		exit(0);
	}

	return pid;
}

int run_parts(char *dir, char *cmd)
{
	struct dirent **e;
	int i, num;

	num = scandir(dir, &e, NULL, alphasort);
	if (num < 0) {
		_d("No files found in %s, skipping ...", dir);
		return -1;
	}

	for (i = 0; i < num; i++) {
		int j = 0, status;
		pid_t pid = 0;
		mode_t mode;
		char *args[NUM_ARGS];
		char *name = e[i]->d_name;
		char path[CMD_SIZE];

		snprintf(path, sizeof(path), "%s/%s", dir, name);
		mode = fmode(path);
		if (!S_ISEXEC(mode) || S_ISDIR(mode)) {
			_d("Skipping %s ...", path);
			continue;
		}

		/* Fill in args[], starting with full path to executable */
		args[j++] = path;

		/* If the callee didn't supply a run_parts() argument */
		if (!cmd) {
			/* Check if S<NUM>service or K<NUM>service notation is used */
			_d("Checking if %s is a sysvinit startstop script ...", name);
			if (name[0] == 'S' && isdigit(name[1])) {
				args[j++] = "start";
			} else if (name[0] == 'K' && isdigit(name[1])) {
				args[j++] = "stop";
			}
		} else {
			args[j++] = cmd;
		}
		args[j++] = NULL;

		pid = fork();
		if (!pid) {
			_d("Calling %s ...", path);
			execv(path, args);
			exit(0);
		}
		waitpid(pid, &status, 0);
	}

	while (num--)
		free(e[num]);
	free(e);

	return 0;
}

int getuser(char *username)
{
	struct passwd *usr;

	if (!username || (usr = getpwnam(username)) == NULL)
		return -1;

	return usr->pw_uid;
}

int getgroup(char *group)
{
	struct group *grp;

	if ((grp = getgrnam(group)) == NULL)
		return -1;

	return grp->gr_gid;
}

/*
 * Other convenience functions
 */

void cls(void)
{
	static const char cls[] = "\e[2J\e[1;1H";

	if (verbose)
		fprintf (stderr, "%s", cls);
}

void chomp(char *str)
{
	char *x;

	if ((x = strchr((str), 0x0a)) != NULL)
		*x = 0;
}

void set_hostname(char *hostname)
{
	FILE *fp;

	_d("Set hostname: %s", hostname);
	if ((fp = fopen("/etc/hostname", "r")) != NULL) {
		fgets(hostname, HOSTNAME_SIZE, fp);
		chomp(hostname);
		fclose(fp);
	}

	sethostname(hostname, strlen(hostname));
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
