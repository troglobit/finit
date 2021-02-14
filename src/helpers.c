/* Misc. utility functions for finit and its plugins
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

static pstyle_t progress_onoff = PROGRESS_DEFAULT;
static pstyle_t progress_style = PROGRESS_DEFAULT;

#ifndef HOSTNAME_PATH
#define HOSTNAME_PATH "/etc/hostname"
#endif

/*
 * Note: the pending status (⋯) must be last item.  Also, ⋯ is not
 *       off-by-one, it's a multi-byte character.
 */
#define STATUS_CLASS {							\
	CHOOSE(" OK ",  " OK ",  "\e[1;32m"),		/* Green  */	\
	CHOOSE("FAIL",  "FAIL",  "\e[1;31m"),		/* Red    */	\
	CHOOSE("WARN",  "WARN",  "\e[1;31m"),		/* Red    */	\
	CHOOSE(" \\/ ", " ⋯  ", "\e[1;33m"),		/* Yellow */	\
}

#define CHOOSE(x,y,z) x
static const char *status1[] = STATUS_CLASS;
#undef CHOOSE

#define CHOOSE(x,y,z) y
static const char *status2[] = STATUS_CLASS;
#undef CHOOSE

#define CHOOSE(x,y,z) z
static const char *color[] = STATUS_CLASS;

/*
 * System consoles, can be given multiple times on the kernel cmdline.
 * We support only three currently.  If none are given on the cmdline
 * we probe /sys/class/tty/console/active and use the first (default).
 */
#define MAX_CONS 3

static char *consoles[MAX_CONS];
static int fds[MAX_CONS];
static int num_cons = 0;

/*
 * System default console is always the first
 */
char *console(void)
{
	return consoles[0];
}

static void add_console(char *cons)
{
	char path[80];
	char *ptr;
	int i;

	if (num_cons == NELEMS(fds)) {
		_e("too many consoles, max %d supported", NELEMS(fds));
		return;
	}

	/* drop any options */
	ptr = strchr(cons, ',');
	if (ptr)
		*ptr = 0;

	snprintf(path, sizeof(path), "/dev/%s", cons);

	/* check for and skip duplicates */
	for (i = 0; i < num_cons; i++) {
		if (string_compare(path, consoles[i]))
			return;
	}

	/* this requires we have /dev mounted && a device there */
	fds[num_cons] = open(path, O_WRONLY);
	if (fds[num_cons] < 0) {
		_pe("Failed opening console %s", path);
		return;
	}
	consoles[num_cons] = strdup(path);
	num_cons++;
}

void console_init(void)
{
	char *cons, *ptr, *tok;
	char buf[512];
	FILE *fp;

	fp = fopen("/sys/class/tty/console/active", "r");
	if (!fp) {
	fallback:
		_d("Using fallback /dev/console");
		add_console("/dev/console");
		return;
	}

	ptr = fgets(buf, sizeof(buf), fp);
	fclose(fp);
	if (!ptr)
		goto fallback;

	cons = chomp(ptr);
	_d("Using these system consoles: %s", cons);
	while ((tok = strtok(cons, " \t"))) {
		cons = NULL;
		add_console(tok);
	}
}

ssize_t cprintf(const char *fmt, ...)
{
	const size_t len = strlen(fmt) * 2;
	char buf[len < 256 ? 256 : len];
	size_t size;
	va_list ap;
	int i;

	va_start(ap, fmt);
	size = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	for (i = 0; i < num_cons; i++)
		(void)write(fds[i], buf, size);

	return size;
}

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

void enable_progress(int onoff)
{
	if (debug)
		return;

	if (onoff)
		progress_style = progress_onoff;
	else
		progress_style = PROGRESS_SILENT;
}

void show_progress(pstyle_t style)
{
	progress_onoff = style;
}

/*
 * Return screen length of string, not counting escape chars, and
 * accounting for unicode characters as only one screen byte wide
 */
size_t slen(char *string)
{
	unsigned char *buf = (unsigned char *)string;
	size_t len = 0;

	while (*buf) {
		/* Skip ANSI escape sequences */
		if (*buf == '\e') {
			while (*buf && !isalpha(*buf))
				buf++;
			continue;
		}
		/* Skip 3-byte unicode */
		if (*buf == 0xe2) {
			for (int cnt = 3; *buf && cnt >= 0; cnt--)
				buf++;
			continue;
		}

		len++;
		buf++;
	}

	return len;
}

/*
 * ch may be an ascii or unicode character
 */
static char *pad(char *buf, size_t len, char *ch, size_t width)
{
	size_t i = slen(buf);

	strlcat(buf, " ", len);

	width -= 8;		/* Skip leading '[ STAT ]' */
	while (i < width) {
		strlcat(buf, ch, len);
		i++;
	}

	return buf;
}

void print_banner(const char *heading)
{
	char buf[4 * SCREEN_WIDTH];

	if (progress_style == PROGRESS_SILENT)
		return;

	memset(buf, 0, sizeof(buf));
	strlcat(buf, "\r\e[2K", sizeof(buf));

	if (progress_style == PROGRESS_CLASSIC) {
		strlcat(buf, "\e[1m", sizeof(buf));
		strlcat(buf, heading, sizeof(buf));
		pad(buf, sizeof(buf), "=", SCREEN_WIDTH + 10);
	} else {
		size_t wmax = 80 <= SCREEN_WIDTH ? 80 : SCREEN_WIDTH;

		/* • • • Foo System ═══════════════════════════════════════════════ */
		/* ⬤ ⬤ ⬤ Foo System ═══════════════════════════════════════════════ */
		/* ⚉ ⚉ ⚉ Foo System ═══════════════════════════════════════════════ */
		/* o o o Foo System ═══════════════════════════════════════════════ */
		/* ◉ ◉ ◉ Foo System ═══════════════════════════════════════════════ */
		/* ⦿⦿⦿ Foo System ═══════════════════════════════════════════════ */
		strlcat(buf, "\e[1;31m● \e[1;33m● \e[1;32m● \e[0m\e[1m ", sizeof(buf));
		strlcat(buf, heading, sizeof(buf));

		/*
		 * Padding with full-width '═' sign from unicode,
		 * we could also use '―' or something else.
		 */
		pad(buf, sizeof(buf), "═", wmax);
	}
	strlcat(buf, "\e[0m\n", sizeof(buf));
	cprintf("%s", buf);
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

static char *status(int rc)
{
	static char buf[64];

	if (rc < 0 || rc >= (int)NELEMS(status1))
		rc = NELEMS(status1) - 1;	/* Default to "⋯" (pending) */

	if (progress_style == PROGRESS_CLASSIC) {
		int hl = 1;

		if (rc == 1 || rc == 2)
			hl = 7;

		snprintf(buf, sizeof(buf), "\e[%dm[%s]\e[0m", hl, status1[rc]);
	} else
		snprintf(buf, sizeof(buf), "\e[1m[%s%s\e[0m\e[1m]\e[0m ", color[rc], status2[rc]);

	return buf;
}

void printv(const char *fmt, va_list ap)
{
	char buf[SCREEN_WIDTH];
	size_t len;

	if (!fmt || progress_style == PROGRESS_SILENT)
		return;

	delline();

	memset(buf, 0, sizeof(buf));
	len = print_timestamp(buf, sizeof(buf));
	vsnprintf(&buf[len], sizeof(buf) - len, fmt, ap);

	if (progress_style == PROGRESS_CLASSIC)
		cprintf("\r%s ", pad(buf, sizeof(buf), ".", sizeof(buf)));
	else
		cprintf("\r\e[2K%s%s", status(3), buf);
}

void print(int rc, const char *fmt, ...)
{
	if (progress_style == PROGRESS_SILENT)
		return;

	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		printv(fmt, ap);
		va_end(ap);
	}

	if (rc < 0)
		return;

	if (progress_style == PROGRESS_CLASSIC)
		cprintf("%s\n", status(rc));
	else
		cprintf("\r%s\n", status(rc));
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

	if (home)
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

	if (rescue)
		goto done;

	fp = fopen(HOSTNAME_PATH, "r");
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

done:
	if (*hostname)
		sethostname(*hostname, strlen(*hostname));
}

/*
 * Bring up networking, but only if not single-user or rescue mode
 */
void networking(int updown)
{
	/* No need to report errors if network is already down */
	if (!prevlevel && !updown)
		return;

	if (updown)
		_d("Setting up networking ...");
	else
		_d("Taking down networking ...");

	/* Run user network start script if enabled */
	if (updown && network) {
		run_interactive(network, "Starting networking: %s", network);
		goto done;
	}

	/* Debian/Ubuntu/Busybox/RH/Suse */
	if (!whichp("ifup"))
		goto done;

	if (fexist("/etc/network/interfaces")) {
		pid_t pid;

		pid = fork();
		if (pid == 0) {
			if (updown)
				run("ifup -a");
			else
				run("ifdown -a");
			_exit(0);
		}

		print(pid > 0 ? 0 : 1, "%s networking", updown ? "Starting" : "Stopping");
	}

done:
	/* Fall back to bring up at least loopback */
	ifconfig("lo", "127.0.0.1", "255.0.0.0", updown);

	/* Hooks that rely on loopback, or basic networking being up. */
	if (updown) {
		_d("Calling all network up hooks ...");
		plugin_run_hooks(HOOK_NETWORK_UP);
	}
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

static int hasopt(char *opts, char *opt)
{
	char buf[strlen(opts) + 1];
	char *ptr;

	memcpy(buf, opts, sizeof(buf));
	ptr = strtok(buf, ",");
	while (ptr) {
		if (!strcmp(ptr, opt))
			return 1;

		ptr = strtok(NULL, ",");
	}

	return 0;
}

int ismnt(char *file, char *dir, char *mode)
{
	struct mntent *mnt;
	int found = 0;
	FILE *fp;

	fp = setmntent(file, "r");
	if (!fp)
		return 0;	/* Dunno, maybe not */

	while ((mnt = getmntent(fp))) {
		if (!strcmp(mnt->mnt_dir, dir)) {
			if (mode && hasopt(mnt->mnt_opts, mode))
				found = 1;
			else
				found = 1;
			break;
		}
	}
	endmntent(fp);

	return found;
}

/* Requires /proc to be mounted */
int fismnt(char *dir)
{
	return ismnt("/proc/mounts", dir, NULL);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
