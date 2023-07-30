#include <ctype.h>		/* isdigit() */
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifdef _LIBITE_LITE
# include <libite/conio.h>
# include <libite/lite.h>
#else
# include <lite/conio.h>
# include <lite/lite.h>
#endif

#include "log.h"
#include "sig.h"
#include "util.h"

#ifdef __FINIT__
#include "helpers.h"
#else
#include <sys/prctl.h>

int debug;

static void print_desc(const char *prefix, const char *msg)
{
	printf("\e[1m[\e[0m\e[1;33m ⋯  \e[0m\e[1m]\e[0m %s %s", prefix, msg);
}
static void print_result(int rc)
{
	if (rc)
		puts("\r\e[1m[\e[0m\e[1;31mFAIL\e[0m\e[1m]\e[0m");
	else
		puts("\r\e[1m[\e[0m\e[1;32m OK \e[0m\e[1m]\e[0m");
}

void sig_unblock(void)
{
	struct sigaction sa;
	sigset_t nmask;
	int i;

	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigaddset(&nmask, SIGCHLD);
	sigaddset(&nmask, SIGINT);
	sigaddset(&nmask, SIGPWR);
	sigaddset(&nmask, SIGSTOP);
	sigaddset(&nmask, SIGTSTP);
	sigaddset(&nmask, SIGCONT);
	sigaddset(&nmask, SIGTERM);
	sigaddset(&nmask, SIGUSR1);
	sigaddset(&nmask, SIGUSR2);
	sigprocmask(SIG_UNBLOCK, &nmask, NULL);

	/* Reset signal handlers that were set by the parent process */
	for (i = 1; i < NSIG; i++)
		DFLSIG(sa, i, 0);
}
#endif /* __FINIT__ */

static void run_env(const char *env[])
{
	size_t i = 0;

	if (!env)
		return;

	while (env[i]) {
		size_t j = i + 1;

		if (!env[j])
			break;

		setenv(env[i], env[j], 1);
		i += 2;
	}
}

int run_parts(char *dir, char *cmd, const char *env[], int progress)
{
	struct dirent **e;
	int i, num;
	int rc = 0;

	num = scandir(dir, &e, NULL, alphasort);
	if (num < 0) {
		dbg("No files found in %s, skipping ...", dir);
		return -1;
	}

	for (i = 0; i < num; i++) {
		const char *name = e[i]->d_name;
		char path[strlen(dir) + strlen(name) + 2];
		struct stat st;
		char *argv[4] = {
			"sh",
			"-c",
			path,
			NULL
		};
		int result = 0;
		pid_t pid = 0;
		int status;

		paste(path, sizeof(path), dir, name);
		if (stat(path, &st)) {
			dbg("Failed stat(%s): %s", path, strerror(errno));
			continue;
		}

		if (!S_ISEXEC(st.st_mode) || S_ISDIR(st.st_mode)) {
			if (strcmp(name, ".") && strcmp(name, ".."))
				dbg("Skipping %s ...", path);
			continue;
		}

		/* If the callee didn't supply a run_parts() argument */
		if (!cmd) {
			/* Check if S<NUM>service or K<NUM>service notation is used */
//			dbg("Checking if %s is a sysvinit startstop script ...", name);
			if (name[0] == 'S' && isdigit(name[1]))
				strlcat(path, " start", sizeof(path));
			else if (name[0] == 'K' && isdigit(name[1]))
				strlcat(path, " stop", sizeof(path));
		} else {
			strlcat(path, cmd, sizeof(path));
		}

		if (progress)
			print_desc("Calling ", path);
		pid = fork();
		if (!pid) {
			sig_unblock();
			run_env(env);

			_exit(execvp(_PATH_BSHELL, argv));
		}

		if (waitpid(pid, &status, 0) == -1) {
			warnx("failed starting %s, error %d: %s", path, errno, strerror(errno));
			result = 1;
		} else {
			if (WIFEXITED(status)) {
				result = WEXITSTATUS(status);
				warnx("%s exited with status %d", path, result);
			} else if (WIFSIGNALED(status)) {
				warnx("%s terminated by signal %d", path, WTERMSIG(status));
				result = 1;
			} else
				result = 1;
		}

		if (progress)
			print_result(result);
		rc += result;
	}

	while (num--)
		free(e[num]);
	free(e);

	return rc;
}

#ifndef __FINIT__
static int usage(int rc)
{
	warnx("usage: runparts [-dh?] DIRECTORY");
	return rc;
}

int main(int argc, char *argv[])
{
	int rc, c, progress = 0;
	char *dir;

	while ((c = getopt(argc, argv, "dh?p")) != EOF) {
		switch(c) {
		case 'd':
			debug = 1;
			break;
		case 'h':
		case '?':
			return usage(0);
		case 'p':
			progress = 1;
			break;
		default:
			return usage(1);
		}
	}

	if (optind >= argc)
		return usage(1);

	dir = argv[optind];
	if (!fisdir(dir))
		return usage(1);

	prctl(PR_SET_CHILD_SUBREAPER, 1);

	rc = run_parts(dir, NULL, NULL, progress);
	if (rc == -1)
		err(1, "failed run-parts %s", dir);

	return rc;
}
#endif /* __FINIT__ */
