/* Misc. utility functions and C-library extensions to simplify finit system setup.
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

#ifdef BUILTIN_RUNPARTS
#include <dirent.h>
#include <stdarg.h>
#include <sys/wait.h>
#endif

#include <grp.h>
#include <pwd.h>

#include "finit.h"
#include "helpers.h"

/*
 * Helpers to replace system() calls
 */

int makepath(char *p)
{
	char *x, path[PATH_MAX];
	int ret;
	
	x = path;

	do {
		do { *x++ = *p++; } while (*p && *p != '/');
		*x = 0;
		ret = mkdir(path, 0777);
	} while (*p && (*p != '/' || *(p + 1))); /* ignore trailing slash */

	return ret;
}

void ifconfig(char *name, char *inet, char *mask, int up)
{
	struct ifreq ifr;
	struct sockaddr_in *a = (struct sockaddr_in *)&ifr.ifr_addr;
	int sock;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		return;

	memset(&ifr, 0, sizeof (ifr));
	strlcpy(ifr.ifr_name, name, IFNAMSIZ);
	a->sin_family = AF_INET;

	if (up) {
		inet_aton(inet, &a->sin_addr);
		ioctl(sock, SIOCSIFADDR, &ifr);
		inet_aton(mask, &a->sin_addr);
		ioctl(sock, SIOCSIFNETMASK, &ifr);
	}

	ioctl(sock, SIOCGIFFLAGS, &ifr);

	if (up)
		ifr.ifr_flags |= IFF_UP;
	else
		ifr.ifr_flags &= ~IFF_UP;

	ioctl(sock, SIOCSIFFLAGS, &ifr);
	
	close(sock);
}

#define BUF_SIZE 4096

void copyfile(char *src, char *dst, int size)
{
	char buffer[BUF_SIZE];
	int s, d, n;

	/* Size == 0 means copy entire file */
	if (size == 0)
		size = INT_MAX;

	if ((s = open(src, O_RDONLY)) >= 0) {
		if ((d = open(dst, O_WRONLY | O_CREAT, 0644)) >= 0) {
			do {
				int csize = size > BUF_SIZE ?  BUF_SIZE : size;
		
				if ((n = read(s, buffer, csize)) > 0)
					write(d, buffer, n);
				size -= csize;
			} while (size > 0 && n == BUF_SIZE);
			close(d);
		}
		close(s);
	}
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

void print_descr(char *action, char *descr)
{
	const char home[] = "\r\e[K";
	const char dots[] = " .....................................................................";

	write(STDERR_FILENO, home, strlen(home));

	print_uptime();

	write(STDERR_FILENO, action, strlen(action));
	write(STDERR_FILENO, descr, strlen(descr));
	write(STDERR_FILENO, dots, 60 - strlen(descr) - strlen(action)); /* pad with dots. */
}

int print_result(int fail)
{
	if (fail)
		fprintf(stderr, " \e[7m[FAIL]\e[0m\n");
	else
		fprintf(stderr, " \e[1m[ OK ]\e[0m\n");

	return fail;
}

int start_process(char *cmd, char *args[], int console)
{
	pid_t pid;
	sigset_t nmask, omask;

	if (sig_stopped())
		return 0;

	/* Block SIGCHLD while forking.  */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	pid = fork();
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (!pid) {
		int i;
		struct sigaction act;

		/* Reset signal handlers that were set by the parent process */
		sigemptyset(&act.sa_mask);
		act.sa_handler = SIG_DFL;

		sigemptyset(&nmask);
		sigaddset(&nmask, SIGCHLD);
		sigprocmask(SIG_UNBLOCK, &nmask, NULL);

		for (i = 1; i < NSIG; i++)
			sigaction(i, &act, NULL);

		if (console) {
			int fd = open (CONSOLE, O_WRONLY | O_APPEND);
			if (-1 != fd) {
				dup2(STDOUT_FILENO, fd);
				dup2(STDERR_FILENO, fd);
			}
		}

		execv(cmd, args);
		exit(!console ? print_result(0) : 0);
	}

	return pid;
}

#ifdef BUILTIN_RUNPARTS

#define NUM_SCRIPTS 128		/* ought to be enough for anyone */
#define NUM_ARGS 16

static int cmp(const void *s1, const void *s2)
{
	return strcmp(*(char **)s1, *(char **)s2);
}

int run_parts(char *dir, ...)
{
	DIR *d;
	struct dirent *e;
	struct stat st;
	char *oldpwd = NULL;
	char *ent[NUM_SCRIPTS];
	int i, num = 0, argnum = 1;
	char *args[NUM_ARGS];
	va_list ap;

	oldpwd = getcwd (NULL, 0);
	if (chdir(dir)) {
		if (oldpwd) free(oldpwd);
		return -1;
	}
	if ((d = opendir(dir)) == NULL) {
		if (oldpwd) free(oldpwd);
		return -1;
	}

	va_start(ap, dir);
	while (argnum < NUM_ARGS && (args[argnum++] = va_arg(ap, char *)));
	va_end(ap);

	while ((e = readdir(d))) {
		if (e->d_type == DT_REG && stat(e->d_name, &st) == 0) {
			_d("Found %s/%s ...", dir, e->d_name);
			if (st.st_mode & S_IXUSR) {
				ent[num++] = strdup(e->d_name);
				if (num >= NUM_SCRIPTS)
					break;
			}
		}
	}

	closedir(d);

	if (num == 0) {
		if (oldpwd) free(oldpwd);
		return 0;
	}

	qsort(ent, num, sizeof(char *), cmp);

	for (i = 0; i < num; i++) {
		pid_t pid = 0;
		int status;

		args[0] = ent[i];
		args[1] = NULL;

		pid = fork();
		if (!pid) {
			_d("Calling %s ...", ent[i]);
			execv(ent[i], args);
			exit(0);
		}
		waitpid(pid, &status, 0);
		free(ent[i]);
	}

	chdir(oldpwd);
	free(oldpwd);

	return 0;
}
#endif /* BUILTIN_RUNPARTS */

int getuser(char *s)
{
	struct passwd *usr;

	if ((usr = getpwnam(s)) == NULL)
		return -1;

	return usr->pw_uid;
}

int getgroup(char *s)
{
	struct group *grp;

	if ((grp = getgrnam(s)) == NULL)
		return -1;

	return grp->gr_gid;
}

/*
 * Other convenience functions
 */

void cls(void)
{
	static const char cls[] = "\e[2J\e[1;1H";

	if (!debug)
		fprintf (stderr, "%s", cls);
}

void chomp(char *s)
{
	char *x;

	if ((x = strchr((s), 0x0a)) != NULL)
		*x = 0;
}

void set_procname(char *args[], char *name)
{
	size_t len = strlen(args[0]) + 1; /* Include terminating '\0' */

	prctl(PR_SET_NAME, name, 0, 0, 0);
	memset(args[0], 0, len);
	strlcpy(args[0], name, len);
}

/**
 * get_pidname - Find name of a process
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
char *get_pidname(pid_t pid, char *name, size_t len)
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
 * kill_procname - Send a signal to a process group by name.
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
int kill_procname (const char *name, int signo)
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
					ERROR("Failed signalling(%d) %s: %s!", signo, name, strerror(error));
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
