/* PID and PID file helpers
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lite/lite.h>

#include "pid.h"
#include "svc.h"
#include "helpers.h"


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

	snprintf(name, sizeof(name), "/proc/%d/status", pid);

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
	char *pname, path[32];
	static char line[64];

	snprintf(path, sizeof(path), "/proc/%d/status", pid);
	fp = fopen(path, "r");
	if (fp) {
		if (fgets(line, sizeof (line), fp)) {
			pname = line + 6; /* Skip first part of line --> "Name:\t" */
			chomp(pname);
			if (name)
				strlcpy(name, pname, len);
			ret = 0;		 /* Found it! */
		}

		fclose(fp);
	}

	if (ret)
		return NULL;

	if (name)
		return name;

	return pname;
}

char *pid_file(svc_t *svc)
{
	static char path[256];
	char fn[MAX_ARG_LEN];

	if (svc->pidfile[0]) {
		if (svc->pidfile[0] == '!')
			return &svc->pidfile[1];
		return svc->pidfile;
	}

	snprintf(fn, sizeof(fn), "%s%s.pid", _PATH_VARRUN, basename(svc->cmd));

	return pid_runpath(fn, path, sizeof(path));
}

pid_t pid_file_read(const char *fn)
{
	pid_t pid = 0;
	FILE *fp;
	char buf[42];

	fp = fopen(fn, "r");
	if (!fp)
		return -1;

	if (fgets(buf, sizeof(buf), fp)) {
		chomp(buf);
		pid = atoi(buf);
	}
	fclose(fp);

	return pid;
}

int pid_file_create(svc_t *svc)
{
	FILE *fp;

	if (!svc->pidfile[0] || svc->pidfile[0] == '!')
		return 1;

	fp = fopen(svc->pidfile, "w");
	if (!fp)
		return 1;
	fprintf(fp, "%d\n", svc->pid);

	return fclose(fp);
}

int pid_file_set(svc_t *svc, char *file, int not)
{
	if (!file) {
		file = pid_file(svc);
		if (!file)
			return -1;
	}

	if (file[0] == '!') {
		not = 1;
		file++;
	}

	pid_runpath(file, &svc->pidfile[not], sizeof(svc->pidfile) - not);
	if (not)
		svc->pidfile[0] = '!';

	return 0;
}

/*
 * This function parses a PID file @arg for @svc.
 *
 * The logic is explained below.  Please note that using the first form
 * of the syntax not only creates and removes the PID file, but it also
 * calls touch to update the mtime on service_restart(), only applicable
 * to processes that accept SIGHUP.  This to ensure dependant services
 * are sent SIGCONT properly on reload, otherwise their condition would
 * never be asserted again after `initctl reload`.
 *
 * pid          --> /run/$(basename).pid
 * pid:foo      --> /run/foo.pid
 * pid:foo.pid  --> /run/foo.pid
 * pid:foo.tla  --> /run/foo.tla.pid
 * pid:/tmp/foo --> /tmp/foo.pid        (Not handled by pidfile plugin!)
 *
 * An exclamation mark, or logial NOT, in the first character position
 * indicates that Finit should *not* manage the PID file, and that the
 * process uses a "non-standard" location.  Eg. if service is 'bar'
 * Finit would assume the PID file is /run/bar.pid and the below simply
 * 'redirects' Finit to use the correct PID file.
 *
 * pid:!foo          --> !/run/foo.pid
 * pid:!/run/foo.pid --> !/run/foo.pid
 *
 * Note, nothing is created or removed by Finit in this latter form.
 */
int pid_file_parse(svc_t *svc, char *arg)
{
	/* Sanity check ... */
	if (!arg || !arg[0])
		return 1;

	/* 'pid:' implies argument following*/
	if (!strncmp(arg, "pid:", 4)) {
		int len, not = 0;
		char path[MAX_ARG_LEN];

		arg += 4;
		if ((arg[0] == '!' && arg[1] == '/') || arg[0] == '/')
			return pid_file_set(svc, arg, 0);

		if (arg[0] == '!') {
			arg++;
			not++;
		}

		len = snprintf(path, sizeof(path), "%s%s%s", not ? "!" : "", _PATH_VARRUN, arg);
		if (len > 4 && strcmp(&path[len - 4], ".pid"))
			strlcat(path, ".pid", sizeof(path));

		return pid_file_set(svc, path, 0);
	}

	/* 'pid' arg, no argument following */
	if (!strcmp(arg, "pid"))
		return pid_file_set(svc, NULL, 0);

	return 1;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
