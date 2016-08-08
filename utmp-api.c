/* UTMP API
 *
 * Copyright (c) 2016  Joachim Nilsson <troglobit@gmail.com>
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

#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <lite/lite.h>

int utmp_set(int type, int pid, char *user, char *line, char *id)
{
	int result;
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
	ut.ut_type  = type;
	ut.ut_pid   = pid;
	strlcpy(ut.ut_user, user, sizeof(ut.ut_user));
	strlcpy(ut.ut_line, line, sizeof(ut.ut_line));
	strlcpy(ut.ut_id, id, sizeof(ut.ut_id));
	if (!uname(&uts))
		strncpy(ut.ut_host, uts.release, sizeof(ut.ut_host));
	ut.ut_tv.tv_sec = time(NULL);

	setutent();
	result = pututline(&ut) ? 0 : 1;;
	endutent();

	return result;
}

static int encode(int lvl)
{
	if (!lvl) return 0;
	return lvl + '0';
}

int utmp_set_runlevel(int pre, int now)
{
	return utmp_set(RUN_LVL, (encode(pre) << 8) | (encode(now) & 0xFF), "runlevel", NULL, NULL);
}

int utmp_set_boot(void)
{
	return utmp_set(BOOT_TIME, 0, "reboot", NULL, NULL);
}

int utmp_set_halt(void)
{
	return utmp_set(RUN_LVL, 0, "shutdown", NULL, NULL);
}

int utmp_show(char *file)
{
	time_t sec;
	struct utmp *ut;
	struct tm *sectm;

	int pid;
	char id[sizeof(ut->ut_id)], user[sizeof(ut->ut_user)], when[80];

	printf("%s =============================================================================================\n", file);
	utmpname(file);

	setutent();
	while ((ut = getutent())) {
		memset(id, 0, sizeof(id));
		strncpy(id, ut->ut_id, sizeof(ut->ut_id));

		memset(user, 0, sizeof(user));
		strncpy(user, ut->ut_user, sizeof(ut->ut_user));

		sec = ut->ut_tv.tv_sec;
		sectm = localtime(&sec);
		strftime(when, sizeof(when), "%F %T", sectm);

		pid = ut->ut_pid;

		printf("[%d] [%05d] [%-4.4s] [%-8.8s] [%-12.12s] [%-20.20s] [%-15.15s] [%-19.19s]\n",
		       ut->ut_type, pid, id, user, ut->ut_line, ut->ut_host, "0.0.0.0", when);
	}
	endutent();

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
