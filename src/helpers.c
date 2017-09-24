/* Misc. utility functions for finit and its plugins
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

#include <ctype.h>		/* isblank() */
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <limits.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <lite/lite.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "util.h"
#include "utmp-api.h"


char *strip_line(char *line)
{
	char *ptr;

	/* Trim leading whitespace */
	while (*line && isblank(*line))
		line++;

	/* Strip any comment at end of line */
	ptr = line;
	while (*ptr && *ptr != '#')
		ptr++;
	*ptr = 0;

	return line;
}

static size_t print_timestamp(char *buf, size_t len)
{
#if defined(CONFIG_PRINTK_TIME)
	FILE *fp;
	float stamp, dummy;

	fp = fopen("/proc/uptime", "r");
	if (!fp)
		return;

	fgets(buf, len, fp);
	fclose(fp);

	sscanf(buf, "%f %f", &stamp, &dummy);
	return snprintf(buf, len, "[ %.6f ]", stamp);
#else
	return 0;
#endif
}

void printv(const char *fmt, va_list ap)
{
	char buf[SCREEN_WIDTH];
	size_t len;

	if (!fmt || log_is_silent())
		return;

	delline();

	memset(buf, 0, sizeof(buf));
	len = print_timestamp(buf, sizeof(buf));
	vsnprintf(&buf[len], sizeof(buf) - len, fmt, ap);

	len = strlen(buf);
	buf[len++] = ' ';
	for (size_t i = len; i < (sizeof(buf) - 8); i++)
		buf[i] = '.';	/* pad with dots. */

	fprintf(stderr, "\r%s ", buf);
}

void print(int action, const char *fmt, ...)
{
	va_list ap;
	const char success[] = "\e[1m[ OK ]\e[0m\n";
	const char failure[] = "\e[7m[FAIL]\e[0m\n";
	const char warning[] = "\e[7m[WARN]\e[0m\n";
	const char pending[] = "\e[1m[ \\/ ]\e[0m\n";

	if (log_is_silent())
		return;

	if (fmt) {
		va_start(ap, fmt);
		printv(fmt, ap);
		va_end(ap);
	}

	switch (action) {
	case -1:
		break;

	case 0:
		write(STDERR_FILENO, success, sizeof(success));
		break;

	case 1:
		write(STDERR_FILENO, failure, sizeof(failure));
		break;

	case 2:
		write(STDERR_FILENO, warning, sizeof(warning));
		break;

	default:
		write(STDERR_FILENO, pending, sizeof(pending));
		break;
	}
}

void print_desc(char *action, char *desc)
{
	print(-1, "%s%s", action ?: "", desc ?: "");
}

int print_result(int fail)
{
	print(!!fail, NULL);
	return fail;
}

int getuser(char *username, char **home)
{
#ifdef ENABLE_STATIC
	*home = "/";		/* XXX: Fixme */
	return fgetint("/etc/passwd", "x:\n", username);
#else
	struct passwd *usr;

	if (!username || (usr = getpwnam(username)) == NULL)
		return -1;

	*home = usr->pw_dir;
	return  usr->pw_uid;
#endif
}

int getgroup(char *group)
{
#ifdef ENABLE_STATIC
	return fgetint("/etc/group", "x:\n", group);
#else
	struct group *grp;

	if ((grp = getgrnam(group)) == NULL)
		return -1;

	return grp->gr_gid;
#endif
}

void set_hostname(char **hostname)
{
	FILE *fp;

	fp = fopen("/etc/hostname", "r");
	if (fp) {
		struct stat st;

		if (fstat(fileno(fp), &st)) {
			fclose(fp);
			return;
		}

		*hostname = realloc(*hostname, st.st_size);
		if (!*hostname) {
			fclose(fp);
			return;
		}

		fgets(*hostname, st.st_size, fp);
		chomp(*hostname);
		fclose(fp);
	}

	if (*hostname)
		sethostname(*hostname, strlen(*hostname));
}

#ifndef HAVE_GETFSENT
static lfile_t *fstab = NULL;

int setfsent(void)
{
	if (fstab)
		lfclose(fstab);

	fstab = lfopen("/etc/fstab", " \t\n");
	if (!fstab)
		return 0;

	return 1;
}

struct fstab *getfsent(void)
{
	static struct fstab fs;

	fs.fs_spec    = lftok(fstab);
	if (fs.fs_spec == NULL)
		return NULL;

	fs.fs_file    = lftok(fstab);
	fs.fs_vfstype = lftok(fstab);
	fs.fs_mntops  = lftok(fstab);
	fs.fs_type    = "rw";
	fs.fs_freq    = atoi(lftok(fstab) ?: "0");
	fs.fs_passno  = atoi(lftok(fstab) ?: "0");

	return &fs;
}

void endfsent(void)
{
	if (fstab)
		lfclose(fstab);

	fstab = NULL;
}
#endif	/* HAVE_GETFSENT */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
