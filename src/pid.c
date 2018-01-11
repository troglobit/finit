/* PID and PID file helpers
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2018  Joachim Nilsson <troglobit@gmail.com>
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
	static char fn[MAX_ARG_LEN];

	if (svc->pidfile[0]) {
		if (svc->pidfile[0] == '!')
			return &svc->pidfile[1];
		return svc->pidfile;
	}

	snprintf(fn, sizeof(fn), "%s%s.pid", _PATH_VARRUN, basename(svc->cmd));
	return fn;
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

int pid_file_parse(svc_t *svc, char *arg)
{
	int len, not = 0;

	/* Always clear, called with some sort of 'pid[:..]' argument */
	svc->pidfile[0] = 0;

	/* Sanity check ... */
	if (!arg || !arg[0])
		return 1;

	/* 'pid:' imples argument following*/
	if (!strncmp(arg, "pid:", 4)) {
		arg += 4;
		if ((arg[0] == '!' && arg[1] == '/') || arg[0] == '/') {
			strlcpy(svc->pidfile, arg, sizeof(svc->pidfile));
			return 0;
		}

		if (arg[0] == '!') {
			arg++;
			not++;
		}
		len = snprintf(svc->pidfile, sizeof(svc->pidfile), "%s%s%s",
			       not ? "!" : "", _PATH_VARRUN, arg);
		if (len > 4 && strcmp(&svc->pidfile[len - 4], ".pid"))
			strlcat(svc->pidfile, ".pid", sizeof(svc->pidfile));

		return 0;
	}

	/* 'pid' arg, no argument following */
	if (!strcmp(arg, "pid")) {
		strlcpy(svc->pidfile, pid_file(svc), sizeof(svc->pidfile));
		return 0;
	}

	return 1;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
