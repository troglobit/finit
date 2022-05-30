/*
 * Basic UNIX daemon
 *
 * Options to run in foreground and to create a PID file.  When running
 * in foregrund it does not create a PID file by default.
 */

#include <err.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PROGNM "serv"

static void verify_env(char *arg)
{
	char *key, *value;
	char *replica;
	char *env;

	key = replica = strdupa(arg);
	if (!replica)
		err(1, "Failed duplicating arg %s", arg);

	value = strchr(replica, ':');
	if (!value)
		errx(1, "Invalid format of KEY:VALUE arg, missing ':' in %s", replica);
	*(value++) = 0;

	env = getenv(key);
	if (!env)
		errx(1, "No '%s' in environment", key);

	if (strcmp(env, value))
		errx(1, "Mismatch, environment '%s' vs expected value '%s'", env, value);
}

static void verify_noenv(char *key)
{
	char *val;

	val = getenv(key);
	if (val)
		errx(1, "Error, key %s is set in environment to '%s'", key, val);
}

static void inc_restarts(void)
{
	char buf[10];
	int cnt = 0;
	FILE *fp;

	fp = fopen("/tmp/serv-restart.cnt", "r+");
	if (!fp) {
		fp = fopen("/tmp/serv-restart.cnt", "w");
		if (!fp)
			err(1, "failed creating restart counter file");
	} else {
		if (fgets(buf, sizeof(buf), fp))
			cnt = atoi(buf);
	}
	cnt++;

	fseek(fp, SEEK_SET, 0);
	fprintf(fp, "%d", cnt);
	fclose(fp);
}

static void sig(int signo)
{
	warnx("We got signal %d ...", signo);
	exit(0);
}

static void pidfile(char *pidfn)
{
	char fn[80];
	pid_t pid;
	FILE *fp;

	if (!pidfn) {
		snprintf(fn, sizeof(fn), "%s%s.pid", _PATH_VARRUN, PROGNM);
		pidfn = fn;
	}
	pid = getpid();
	warnx("Creating PID file %s with %d", pidfn, pid);

	fp = fopen(pidfn, "w");
	if (!fp)
		exit(1);
	fprintf(fp, "%d\n", pid);
	fclose(fp);
}

static int usage(int rc)
{
	FILE *fp = rc ? stderr : stdout;

	fprintf(fp,
		"%s [-nhp] [-P FILE]\n"
		"\n"
		" -h       Show help text (this)\n"
		" -e K:V   Verify K environment variable is V value\n"
		" -E K     Verify K is not set in the environment\n"
		" -n       Run in foreground\n"
		" -p       Create PID file despite running in foreground\n"
		" -P FILE  Create PID file using FILE\n"
		" -r SVC   Call initctl to restart service SVC (self)\n"
		"\n"
		"By default this program daemonizes itself to the background, and,\n"
		"when it's done setting up its signal handler(s), creates a PID file\n"
		"to let the rest of the system know it's done.  When the program runs\n"
		"in the foreground it does not create a PID file by default.\n",
		PROGNM);

	return rc;
}

int main(int argc, char *argv[])
{
	int do_background = 1;
	int do_pidfile = 1;
	int do_restart = 0;
	char *pidfn = NULL;
	char cmd[80];
	int c;

	while ((c = getopt(argc, argv, "e:E:hnpP:r:")) != EOF) {
		switch (c) {
		case 'h':
			return usage(0);
		case 'e':
			verify_env(optarg);
			break;
		case 'E':
			verify_noenv(optarg);
			break;
		case 'n':
			do_background = 0;
			do_pidfile--;
			break;
		case 'p':
			do_pidfile++;
			break;
		case 'P':
			pidfn = optarg;
			break;
		case 'r':
			snprintf(cmd, sizeof(cmd), "initctl restart %s", optarg);
			do_restart = 1;
			break;
		default:
			return usage(1);
		}
	}

	/* Daemonize, fork to background etc. */
	if (do_background) {
		if (daemon(0, 1))
			return 1;
	}

	/* Signal handlers first *then* PID file */
	signal(SIGTERM, sig);

	/* Tell world where we are, but not if bg w/o pid file */
	if (do_pidfile == 1)
		pidfile(pidfn);

	warnx("Entering while(1) loop");
	while (1) {
		sleep(1);
		if (do_restart) {
			inc_restarts();
			if (system(cmd))
				return 1;
			break;
		}
	}

	warnx("Leaving ...");
	return 0;
}
