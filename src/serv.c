/* List and enable/disable service configurations
 *
 * Copyright (c) 2017-2025  Joachim Wiberg <troglobit@gmail.com>
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

#include <fcntl.h> /* Definition of AT_* constants */
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "client.h"
#include "initctl.h"


static int is_builtin(char *arg)
{
	svc_t *svc;

	/* skip check, we may be in non-responsive bootstrap */
	if (iforce)
		return 0;

	svc = client_svc_find(arg);
	if (!svc)
		return 0;

	if (svc->file[0])
		return 0;

	return 1;
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

static void do_list(const char *path, char *title, int comma)
{
	static int once = 0;
	int width, num, prev;
	glob_t gl;
	size_t i;

	if (chdir(path)) {
		char *dir, *ptr;

		if (!fexist(path))
			return;

		dir = strdupa(path);
		if (!dir)
			return;

		ptr = strrchr(dir, '/');
		if (ptr)
			*ptr++ = 0;
		else
			ptr = dir;

		if (json) {
			printf("%s\"%s\": \"%s\"", comma ? "," :"", title, path);
			return;
		}

		if (heading)
			print_header("%s%s ", once ? "\n" : "", dir);
		puts(ptr);
		puts("");
		return;
	}

	if (glob("*.conf", 0, NULL, &gl))
		return;

	if (gl.gl_pathc <= 0)
		goto done;

	if (plain && !json) {
		if (heading)
			print_header("%s%s ", once ? "\n" : "", path);
		once++;

		for (i = 0; i < gl.gl_pathc; i++) {
			if (!heading) {
				char buf[512];

				paste(buf, sizeof(buf), path, gl.gl_pathv[i]);
				puts(buf);
			} else
				puts(gl.gl_pathv[i]);
		}

		goto done;
	}

	if (heading && !json)
		print_header("%s ", path);
	if (json)
		printf("%s\"%s\": [", comma ? "," : "", title);

	width = calc_width(gl.gl_pathv, gl.gl_pathc);
	if (width <= 0)
		goto done;

	num = (ttcols - 2) / width;
	if ((num - 1) * 2 + num * width > ttcols)
		num--;

	prev = 0;
	for (i = 0; i < gl.gl_pathc; i++) {
		if (i > 0 && !(i % num) && !json) {
			puts("");
			prev = 0;
		}

		if (prev)
			printf("%s", json ? "," : "  ");
		if (json)
			printf("\"%s\"", gl.gl_pathv[i]);
		else
			printf("%-*s", width, gl.gl_pathv[i]);
		prev++;
	}

	if (json)
		fputs("]", stdout);
	else
		puts("\n");

done:
	globfree(&gl);
}

int serv_list(char *arg)
{
	char path[256];
	int pre = 0;

	if (arg && arg[0]) {
		paste(path, sizeof(path), finit_rcsd, arg);
		if (fisdir(path)) {
			if (json)
				fputs("[", stdout);
			do_list(path, NULL, 0);
			if (json)
				fputs("]\n", stdout);
			return 0;
		}
		/* fall back to list all */
	}

	if (json)
		fputs("{", stdout);

	paste(path, sizeof(path), finit_rcsd, "available");
	if (fisdir(path))
		do_list(path, "available", pre++);

	paste(path, sizeof(path), finit_rcsd, "enabled");
	if (fisdir(path))
		do_list(path, "enabled", pre++);

	if (fisdir(finit_rcsd))
		do_list(finit_rcsd, "static", pre++);

	if (fexist(finit_conf))
		do_list(finit_conf, "finit.conf", pre++);

	if (json)
		fputs("}\n", stdout);

	return 0;
}

/*
 * Return path to configuration file for 'name'.  This may be any of the
 * following, provided sysconfdir is /etc:
 *
 *   - /etc/finit.d/available/$name.conf
 *   - /etc/finit.d/$name.conf
 *   - /etc/finit.conf
 *
 * The system *may* have a /etc/finit.d/available/ directory, or it may
 * just use a plain /etc/finit.d/ -- we do not set policy.
 *
 * If the resulting file doesn't exist, and creat is not set, *or*
 * the base directory doesn't exist, we return NULL.
.*/
static char *conf(char *path, size_t len, char *name, int creat, int template)
{
	if (!name || !name[0] || !strcmp(name, "finit") || !strcmp(name, "finit.conf")) {
		strlcpy(path, finit_conf, len);
		return path;
	}

	if (!fisdir(finit_rcsd))
		return NULL;

	paste(path, len, finit_rcsd, "available/");
	if (!fisdir(path)) {
		if (creat && mkdir(path, 0755) && errno != EEXIST)
			return NULL;
		paste(path, len, finit_rcsd, name);
	} else
		strlcat(path, name, len);

	if (suffix(path, len, ".conf"))
		return NULL;

	/* fall back to static service unless edit/create */
	if (!creat && !fexist(path)) {
		char *at = strchr(path, '@');

		if (template && at)
			*++at = 0;
		else
			paste(path, len, finit_rcsd, name);

		if (suffix(path, len, ".conf"))
			return NULL;
	}

	return path;
}

int serv_enable(char *arg)
{
	char *serv, *ptr, *src, *dst;
	size_t len;
	int rc;

	if (!arg || !arg[0]) {
		WARNX("missing argument to enable, may be one of:");
		return serv_list("available");
	}

	if (arg[0] == '/') {
		WARNX("absolute paths not supported, this command manages %s/available/", finit_rcsd);
		return serv_list("available");
	}

	len = strlen(arg);
	serv = alloca(len + 6);
	if (!serv)
		ERR(71, "failed allocating stack");

	strlcpy(serv, arg, len + 6);
	if (len < 6 || strcmp(&serv[len - 5], ".conf"))
		strlcat(serv, ".conf", len + 6);

	if (chdir(finit_rcsd))
		ERR(72, "failed cd %s", finit_rcsd);

	if (icreate && mkdir("enabled", 0755) && EEXIST != errno)
		ERR(73, "failed creating %s/enabled directory", finit_rcsd);

	if (asprintf(&src, "%s/available/%s", finit_rcsd, serv) == -1)
		goto fail;
	if (asprintf(&dst, "%s/enabled/%s", finit_rcsd, serv) == -1) {
		free(src);
	fail:
		ERR(71, "failed allocating memory");
	}

	if (fexist(dst)) {
		WARNX("%s already enabled", arg);
		rc = noerr ? 0 : 1;
		goto done;
	}

	/* check for template instantiation */
	ptr = strchr(src, '@');
	if (ptr)
		strcpy(++ptr, ".conf");

	if (!fexist(src)) {
		WARNX("cannot find %s%s", ptr ? "template ": "", src);
		rc = 72;
		goto done;
	}

	rc = symlink(src, dst);
	if (rc)
		WARN("failed enabling %s", arg);
done:
	free(src);
	free(dst);
	return rc;
}

int do_disable(char *arg, int check)
{
	struct stat st;
	size_t len;
	char *serv;

	if (!arg || !arg[0]) {
		WARNX("missing argument to disable, may be one of:");
		return serv_list("enabled");
	}

	if (arg[0] == '/') {
		WARNX("absolute paths not supported, this command manages %s/enabled/", finit_rcsd);
		return serv_list("available");
	}

	len = strlen(arg) + 7;
	serv = alloca(len);
	if (!serv)
		ERR(71, "failed allocating stack");

	strlcpy(serv, arg, len);
	if (suffix(serv, len, ".conf"))
		ERR(71, "failed composing service name (%s)", serv);

	if (chdir(finit_rcsd))
		ERR(72, "failed cd %s", finit_rcsd);
	if (chdir("enabled"))	   /* System *may* have enabled/ dir. */
		dbg("Failed changing to %s/enabled/: %s", finit_rcsd, strerror(errno));

	if (check && stat(serv, &st))
		ERRX(noerr ? 0 : 6, "%s not (an) enabled (service).", serv);

	if (check && (st.st_mode & S_IFMT) == S_IFLNK)
		ERRX(noerr ? 0 : 1, "cannot disable %s, not a symlink.", serv);

	return remove(serv) != 0;
}

int serv_disable(char *arg)
{
	return do_disable(arg, 1);
}

int serv_touch(char *arg)
{
	char path[256];
	char *fn;

	if (!arg || !arg[0]) {
		WARNX("missing argument to touch, may be one of:");
		return serv_list("enabled");
	}

	/* 1. Try /etc/finit.d/enabled/$arg.conf to handle template@.conf */
	paste(path, sizeof(path), finit_rcsd, "enabled/");
	strlcat(path, arg, sizeof(path));
	if (!suffix(path, sizeof(path), ".conf") && fexist(path)) {
		fn = path;
		goto touchit;
	}

	/* 2. Try /etc/finit.d/available/$arg.conf and other combos (legacy) */
	fn = conf(path, sizeof(path), arg, 0, 0);
	if (!fexist(fn)) {
		if (!strstr(arg, "finit.conf"))
			ERRX(noerr ? 0 : 72, "%s not available.", arg);
		if (is_builtin(arg))
			ERRX(noerr ? 0 : 4, "%s is a built-in service.", arg);

		strlcpy(path, finit_conf, sizeof(path));
		fn = path;
	}

touchit:
	/* libite:touch() follows symlinks */
	if (utimensat(AT_FDCWD, fn, NULL, AT_SYMLINK_NOFOLLOW))
		ERR(noerr ? 0 : 71, "failed marking %s for reload", fn);

	return 0;
}

int serv_show(char *arg)
{
	char path[256];
	char *fn;

	fn = conf(path, sizeof(path), arg, 0, 0);
	if (!fexist(fn)) {
		if (is_builtin(arg))
			ERRX(4, "%s is a built-in service.", arg);

		fn = conf(path, sizeof(path), arg, 0, 1);
		if (!fn || !fexist(fn)) {
			WARNX("Cannot find %s", arg);
			return 1;
		}

		return systemf("cat %s | sed 's/%%i/system/g'", fn);
	}

	return systemf("cat %s", fn);
}

/*
 * Try to open an editor for the given file, if creat is given we
 * create a new file based on /lib/finit/sample.conf
 *
 * The order of editors that this command checks for is evaluated
 * as follows, in order:
 *
 *     sensible-editor :: debian based systems
 *     editor          :: debian + most other systems
 *     VISUAL          :: full-screen editor program
 *     EDITOR          :: line-mode editor program
 *     $(command ...)  :: fallback to mg or vi
 *
 * For details: https://jdebp.uk/FGA/unix-editors-and-pagers.html
 */
static int do_edit(char *arg, int creat)
{
	char *editor[] = {
		"sensible-editor",
		"editor",
		"${VISUAL:-${EDITOR:-$(command -v mg || command -v vi)}}"
	};
	char path[256];
	char *fn;

	fn = conf(path, sizeof(path), arg, creat, 0);
	if (!fexist(fn)) {
		if (is_builtin(arg))
			ERRX(4, "%s is a built-in service.", arg);

		if (!creat) {
			WARNX("Cannot find %s, use -c flag, create command, or select one of:", arg);
			return serv_list(NULL);
		}

#ifdef SAMPLE_CONF
		/* Try copying file, it may have been removed by sysop */
		copyfile(SAMPLE_CONF, fn, 0, 0);
#endif
	} else if (creat)
		WARNX("the file %s already exists, falling back to edit.", fn);

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
		if (!yorn("Do you want to edit %s (y/N)? ", finit_conf))
			return 0;
		arg = "";
	}

	return do_edit(arg, icreate);
}

int serv_creat(char *arg)
{
	char buf[256];
	char *fn;
	FILE *fp;

	if (!arg || !arg[0])
		ERRX(2, "missing argument to create");

	if (is_builtin(arg))
		ERRX(4, "%s is a built-in service.", arg);

	/* Input from a pipe or a proper TTY? */
	if (isatty(STDIN_FILENO))
		return do_edit(arg, 1);

	/* Open fn for writing from pipe */
	fn = conf(buf, sizeof(buf), arg, 1, 0);
	if (!fn)
		ERR(73, "failed creating conf %s", arg);

	if (!icreate && fexist(fn)) {
		WARNX("%s already exists, skipping (use -c to override)", fn);
		fn = "/dev/null";
	}

	fp = fopen(fn, "w");
	if (!fp)
		ERR(73, "failed opening %s for writing", fn);

	while (fgets(buf, sizeof(buf), stdin))
		fputs(buf, fp);

	return fclose(fp);
}

int serv_delete(char *arg)
{
	char buf[256];
	char *fn;

	if (!arg || !arg[0]) {
		WARNX("missing argument to delete, may be one of:");
		return serv_list("available");
	}

	fn = conf(buf, sizeof(buf), arg, 0, 0);
	if (!fn) {
		if (is_builtin(arg))
			ERRX(4, "%s is a built-in service.", arg);
		ERRX(72, "%s missing on system.", finit_rcsd);
	}

	if (!fexist(fn))
		WARNX("cannot find %s", fn);

	if (iforce || yorn("Remove file and symlink(s) to %s (y/N)? ", fn)) {
		do_disable(arg, 0);
		if (remove(fn))
			ERR(1, "Failed removing %s", fn);
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
