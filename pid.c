/* Misc. utility functions and C-library extensions for finit and its plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "helpers.h"
#include "libite/lite.h"


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
		pid = (pid_t)strtol(buf, NULL, 10);
		if (errno)
			pid = 0;		/* Failed conversion. */
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

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
