/* UTMP API
 *
 * Copyright (c) 2016-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <paths.h>
#include <time.h>
#include <utmp.h>
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <lite/lite.h>

#include "helpers.h"

#ifndef _PATH_BTMP
#define _PATH_BTMP "/var/log/btmp"
#endif

#define MAX_NO 5
#define MAX_SZ 100 * 1024

static void utmp_strncpy(char *dst, const char *src, size_t dlen)
{
	size_t i;

	for (i = 0; i < dlen && src[i]; i++)
		dst[i] = src[i];

	if (i < dlen)
		dst[i] = 0;
}

#ifdef LOGROTATE_ENABLED
/*
 * This function triggers a log rotates of @file when size >= @sz bytes
 * At most @num old versions are kept and by default it starts gzipping
 * .2 and older log files.  If gzip is not available in $PATH then @num
 * files are kept uncompressed.
 */
static int logrotate(char *file, int num, off_t sz)
{
	int cnt;
	struct stat st;

	if (stat(file, &st))
		return 1;

	if (sz > 0 && S_ISREG(st.st_mode) && st.st_size > sz) {
		if (num > 0) {
			size_t len = strlen(file) + 10 + 1;
			char   ofile[len];
			char   nfile[len];

			/* First age zipped log files */
			for (cnt = num; cnt > 2; cnt--) {
				snprintf(ofile, len, "%s.%d.gz", file, cnt - 1);
				snprintf(nfile, len, "%s.%d.gz", file, cnt);

				/* May fail because ofile doesn't exist yet, ignore. */
				(void)rename(ofile, nfile);
			}

			for (cnt = num; cnt > 0; cnt--) {
				snprintf(ofile, len, "%s.%d", file, cnt - 1);
				snprintf(nfile, len, "%s.%d", file, cnt);

				/* May fail because ofile doesn't exist yet, ignore. */
				(void)rename(ofile, nfile);

				if (cnt == 2 && fexist(nfile)) {
					size_t len = 5 + strlen(nfile) + 1;
					char cmd[len];

					snprintf(cmd, len, "gzip %s", nfile);
					run(cmd);

					remove(nfile);
				}
			}

			(void)rename(file, nfile);
			create(file, st.st_mode, st.st_uid, st.st_gid);
		} else {
			truncate(file, 0);
		}
	}

	return 0;
}
#endif /* LOGROTATE_ENABLED */

/*
 * Rotate /var/log/wtmp (+ btmp?) and /run/utmp
 *
 * Useful on systems with no logrotate daemon, e.g. BusyBox based
 * systems where syslogd rotates its own log files only.
 */
void utmp_logrotate(void)
{
#ifdef LOGROTATE_ENABLED
	int i;
	char *files[] = {
		_PATH_UTMP,
		_PATH_WTMP,
		_PATH_BTMP,
		_PATH_LASTLOG,
		NULL
	};

	for (i = 0; files[i]; i++)
		logrotate(files[i], MAX_NO, MAX_SZ);
#endif /* LOGROTATE_ENABLED */
}

int utmp_set(int type, int pid, char *line, char *id, char *user)
{
	int result = 0;
	struct utmp ut;
	struct utsname uts;

	switch (type) {
	case RUN_LVL:
	case BOOT_TIME:
		line = "~";
		id   = "~~";
		break;

	default:
		break;
	}

	memset(&ut, 0, sizeof(ut));
	ut.ut_type = type;
	ut.ut_pid  = pid;
	if (user)
		utmp_strncpy(ut.ut_user, user, sizeof(ut.ut_user));
	if (line)
		utmp_strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	if (id)
		utmp_strncpy(ut.ut_id, id, sizeof(ut.ut_id));
	if (!uname(&uts))
		utmp_strncpy(ut.ut_host, uts.release, sizeof(ut.ut_host));
	ut.ut_tv.tv_sec = time(NULL);

	if (type != DEAD_PROCESS) {
		setutent();
		result += pututline(&ut) ? 0 : 1;
		endutent();
	}

	utmp_logrotate();
	updwtmp(_PATH_WTMP, &ut);

	return result;
}

int utmp_set_boot(void)
{
	return utmp_set(BOOT_TIME, 0, NULL, NULL, "reboot");
}

int utmp_set_halt(void)
{
	return utmp_set(RUN_LVL, 0, NULL, NULL, "shutdown");
}

static int set_getty(int type, char *tty, char *id, char *user)
{
	char *line;

	if (!tty)
		line = "";
	else if (!strncmp(tty, "/dev/", 5))
		line = tty + 5;
	else
		line = tty;

	if (!id) {
		id = line;
		if (!strncmp(id, "pts/", 4))
			id += 4;
	}

	return utmp_set(type, getpid(), line, id, user);
}

int utmp_set_init(char *tty, char *id)
{
	return set_getty(INIT_PROCESS, tty, id, NULL);
}

int utmp_set_login(char *tty, char *id)
{
	return set_getty(LOGIN_PROCESS, tty, id, "LOGIN");
}

int utmp_set_dead(int pid)
{
	return utmp_set(DEAD_PROCESS, pid, NULL, NULL, NULL);
}

static int encode(int lvl)
{
	if (!lvl) return 0;
	return lvl + '0';
}

int utmp_set_runlevel(int pre, int now)
{
	return utmp_set(RUN_LVL, (encode(pre) << 8) | (encode(now) & 0xFF), NULL, NULL, "runlevel");
}

void runlevel_set(int pre, int now)
{
	utmp_set_runlevel(pre, now);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
