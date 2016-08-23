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
#include <arpa/inet.h>
#include <sys/utsname.h>
#include <lite/lite.h>

int utmp_set(int type, int pid, char *line, char *id, char *user)
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
	ut.ut_type = type;
	ut.ut_pid  = pid;
	if (user)
		strncpy(ut.ut_user, user, sizeof(ut.ut_user));
	if (line)
		strncpy(ut.ut_line, line, sizeof(ut.ut_line));
	if (id)
		strncpy(ut.ut_id, id, sizeof(ut.ut_id));
	if (!uname(&uts))
		strncpy(ut.ut_host, uts.release, sizeof(ut.ut_host));
	ut.ut_tv.tv_sec = time(NULL);

	setutent();
	while (getutent())
		;
	result = pututline(&ut) ? 0 : 1;;
	updwtmp(_PATH_WTMP, &ut);
	endutent();

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
		char addr[64];

		memset(id, 0, sizeof(id));
		strncpy(id, ut->ut_id, sizeof(ut->ut_id));

		memset(user, 0, sizeof(user));
		strncpy(user, ut->ut_user, sizeof(ut->ut_user));

		sec = ut->ut_tv.tv_sec;
		sectm = localtime(&sec);
		strftime(when, sizeof(when), "%F %T", sectm);

		pid = ut->ut_pid;
		if (ut->ut_addr_v6[1] || ut->ut_addr_v6[2] || ut->ut_addr_v6[3])
			inet_ntop(AF_INET6, ut->ut_addr_v6, addr, sizeof(addr));
		else
			inet_ntop(AF_INET, &ut->ut_addr, addr, sizeof(addr));

		printf("[%d] [%05d] [%-4.4s] [%-8.8s] [%-12.12s] [%-20.20s] [%-15.15s] [%-19.19s]\n",
		       ut->ut_type, pid, id, user, ut->ut_line, ut->ut_host, addr, when);
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
