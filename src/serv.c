/* List and enable/disable service configurations
 *
 * Copyright (c) 2017-2020  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"

#include <err.h>
#include <fcntl.h> /* Definition of AT_* constants */
#include <glob.h>
#include <stdio.h>
#include <lite/lite.h>

#include "util.h"

static const char *cwd;
static const char *available = FINIT_RCSD "/available";
static const char *enabled   = FINIT_RCSD "/enabled";

static void pushd(const char *dir)
{
	cwd = get_current_dir_name();
	chdir(dir);
}

static void popd(void)
{
	chdir(cwd);
}

static int calc_width(char *arr[], size_t len)
{
	int width = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		int w = strlen(arr[i]);

		if (w > width)
			width = w;
	}

	return width;
}

static void do_list(const char *path)
{
 	char buf[screen_cols];
	int width, num, prev;
	glob_t gl;
	size_t i;

	pushd(path);
	if (glob("*.conf", 0, NULL, &gl)) {
		chdir(cwd);
		return;
	}

	if (gl.gl_pathc <= 0)
		goto done;

	snprintf(buf, sizeof(buf), "%s ", path);
	printheader(NULL, buf, 0);

	width = calc_width(gl.gl_pathv, gl.gl_pathc);
	if (width <= 0)
		goto done;

	num = (screen_cols - 2) / width;
	if ((num - 1) * 2 + num * width > screen_cols)
		num--;

	prev = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
		if (i > 0 && !(i % num)) {
			puts("");
			prev = 0;
		}

		if (prev)
			printf("  ");
		printf("%-*s", width, gl.gl_pathv[i]);
		prev++;
	}
	puts("\n");

done:
	globfree(&gl);
	popd();
}

int serv_list(char *arg)
{
	if (fisdir(available))
		do_list(available);
	if (fisdir(enabled))
		do_list(enabled);
	if (fisdir(FINIT_RCSD))
		do_list(FINIT_RCSD);

	return 0;
}

int serv_enable(char *arg)
{
	char corr[40];
	char link[256];
	char path[256];

	if (!arg || !arg[0])
		return serv_list(NULL);

	if (!strstr(arg, ".conf")) {
		snprintf(corr, sizeof(corr), "%s.conf", arg);
		arg = corr;
	}

	pushd(FINIT_RCSD);
	if (mkdir("enabled", 0755) && EEXIST != errno)
		err(1, "Failed creating %s/enabled directory", FINIT_RCSD);

	snprintf(path, sizeof(path), "%s/%s", available, arg);
	if (!fexist(path))
		errx(1, "Cannot find %s", path);

	snprintf(link, sizeof(link), "%s/%s", enabled, arg);
	if (fexist(link))
		errx(1, "%s already exists", link);

	return symlink(path, link) != 0;
}

int serv_disable(char *arg)
{
	char corr[40];
	char link[256];
	struct stat st;

	if (!arg || !arg[0])
		return serv_list(NULL);

	if (!strstr(arg, ".conf")) {
		snprintf(corr, sizeof(corr), "%s.conf", arg);
		arg = corr;
	}

	snprintf(link, sizeof(link), "%s/%s", enabled, arg);
	if (stat(link, &st))
		err(1, "Cannot find %s", link);

	if ((st.st_mode & S_IFMT) == S_IFLNK)
		errx(1, "%s is not a symlink, move manually to %s first", link, available);

	return remove(link) != 0;
}

int serv_touch(char *arg)
{
	char corr[40];

	if (!arg || !arg[0])
		return serv_list(NULL);

	if (!strstr(arg, ".conf")) {
		snprintf(corr, sizeof(corr), "%s.conf", arg);
		arg = corr;
	}

	pushd(FINIT_RCSD);
	if (!fexist(arg)) {
		popd();
		if (!strstr(arg, "finit.conf"))
			errx(1, "Service %s is not enabled", arg);
		arg = FINIT_CONF;
	}

	/* libite:touch() follows symlinks */
	if (utimensat(AT_FDCWD, arg, NULL, AT_SYMLINK_NOFOLLOW))
		err(1, "Failed marking %s for reload", arg);

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
