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

#ifndef FINIT_PID_H_
#define FINIT_PID_H_

#include "svc.h"

int   pid_alive       (pid_t pid);
char *pid_get_name    (pid_t pid, char *name, size_t len);

char *pid_file        (svc_t *svc);
int   pid_file_create (svc_t *svc);
int   pid_file_parse  (svc_t *svc, char *arg);

/**
 * pid_runpath - Adjust /var/run --> /run path depending on system
 * @file: Path to file in /run/path or /var/run/path
 * @path: Pointer to buffer to write correct path
 * @len:  Length, in bytes, of @path buffer
 *
 * Returns:
 * Always returns a valid pointer, which can be either @path with a /run
 * or /var/run prefix to @file, or it may be @file if the prefix is OK.
 */
static inline char *pid_runpath(const char *file, char *path, size_t len)
{
	static int unknown = 1;
	static char *prefix = "/var/run";

	if (unknown) {
		if (fisdir("/run"))
			prefix = "/run";
		unknown = 0;
	}

	if (!strncmp(file, prefix, strlen(prefix)))
		return (char *)file;

	if (!strncmp(file, "/var/run/", 9))
		file += 9;
	else if (!strncmp(file, "/run/", 5))
		file += 5;

	snprintf(path, len, "%s/%s", prefix, file);

	return path;
}

#endif /* FINIT_PID_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
