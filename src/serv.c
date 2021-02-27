/* List and enable/disable service configurations
 *
 * Copyright (c) 2017-2021  Joachim Wiberg <troglobit@gmail.com>
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
#include <stdlib.h>
#include <lite/lite.h>

#include "util.h"

static const char *cwd;
static const char *available = FINIT_RCSD "/available";
static const char *enabled   = FINIT_RCSD "/enabled";
extern int icreate;			/* initctl -c */

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

/*
 * Return path to configuration file for 'name', relative to FINIT_RCSD.
 * This may be any of the following, provided sysconfdir is /etc:
 *
 *   - /etc/finit.d/$name.conf
 *   - /etc/finit.d/available/$name.conf
 *
 * The system *may* have a /etc/finit.d/available/ directory, or it may
 * just use a plain /etc/finit.d/ -- we do not set policy.
 *
 * If the resulting file doesn't exist, and creat is not set, *or*
 * the base directory doesn't exist, we return NULL.
.*/
static char *conf(char *path, size_t len, char *name, int creat)
{
	char corr[40];

	if (!strstr(name, ".conf")) {
		snprintf(corr, sizeof(corr), "%s.conf", name);
		name = corr;
	}

	if (!fisdir(FINIT_RCSD))
		return NULL;

	paste(path, len, FINIT_RCSD, "available/");
	if (!fisdir(path)) {
		if (creat && mkdir(path, 0755) && errno != EEXIST)
			return NULL;

		paste(path, len, FINIT_RCSD, name);
	} else
		strlcat(path, name, len);

	return path;
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
	char path[256];
	char *fn;

	if (!arg || !arg[0]) {
		warnx("missing argument to touch, may be one of:");
		return serv_list("enabled");
	}

	fn = conf(path, sizeof(path), arg, 0);
	if (!fexist(fn)) {
		if (!strstr(arg, "finit.conf"))
			errx(1, "%s not available.", arg);

		strlcpy(path, FINIT_CONF, sizeof(path));
		fn = path;
	}

	/* libite:touch() follows symlinks */
	if (utimensat(AT_FDCWD, fn, NULL, AT_SYMLINK_NOFOLLOW))
		err(1, "failed marking %s for reload", fn);

	return 0;
}

static int do_edit(char *arg, int creat)
{
	char *editor[] = {
		"sensible-editor",
		"editor",
		"${VISUAL:-${EDITOR}}",
		"mg",
		"vi"
	};
	char path[256];
	char *fn;

	fn = conf(path, sizeof(path), arg, creat);
	if (!fexist(fn)) {
		if (!creat) {
			warnx("Cannot find %s, use create command, or select one of:", arg);
			return serv_list(NULL);
		}

		/* XXX: fill with template/commented-out examples */
	} else if (creat)
		warnx("the file %s already exists, falling back to edit.", fn);

	for (size_t i = 0; i < NELEMS(editor); i++) {
		if (systemf("%s %s 2>/dev/null", editor[i], path))
			continue;
		return 0;
	}

	return 1;
}

int serv_edit(char *arg)
{
	if (!arg || !arg[0]) {
		warnx("missing argument to edit, may be one of:");
		return serv_list("available");
	}

	return do_edit(arg, icreate);
}

int serv_creat(char *arg)
{
	char buf[256];
	char *fn;
	FILE *fp;

	if (!arg || !arg[0])
		errx(1, "missing argument to create");

	/* Input from a pipe or a proper TTY? */
	if (isatty(STDIN_FILENO))
		return do_edit(arg, 1);

	/* Open fn for writing from pipe */
	fn = conf(buf, sizeof(buf), arg, 1);
	if (!fn)
		err(1, "failed creating conf %s", arg);

	if (!icreate && fexist(fn)) {
		warnx("%s already exists, skipping (use -c to override)", fn);
		fn = "/dev/null";
	}

	fp = fopen(fn, "w");
	if (!fp)
		err(1, "failed opening %s for writing", fn);

	while (fgets(buf, sizeof(buf), stdin))
		fputs(buf, fp);

	return fclose(fp);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
