/* Basic forking UNIX daemon with no options to run in foreground */

#include <err.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void sig(int signo)
{
	warnx("We got signal %d ...", signo);
	exit(0);
}

static void pidfile(char *nm)
{
	char fn[80];
	pid_t pid;
	FILE *fp;

	if (!nm)
		nm = "fserv";

	snprintf(fn, sizeof(fn), "%s%s.pid", _PATH_VARRUN, nm);
	pid = getpid();
	warnx("Creating PID file %s with %d", fn, pid);

	fp = fopen(fn, "w");
	if (!fp)
		exit(1);
	fprintf(fp, "%d\n", pid);
	fclose(fp);
}

int main(void)
{
	/* Daemonize, fork to background etc. */
	if (daemon(0, 1))
		return 1;

	/* Signal handlers first *then* PID file */
	signal(SIGTERM, sig);

	/* Tell finit where we really are */
	pidfile(NULL);

	warnx("Entering while(1) loop");
	while (1)
		sleep(1);

	warnx("Leaving ...");
	return 0;
}
