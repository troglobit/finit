/* poor man's sulogin, may be statically linked
 *
 * Copyright (c) 2021-2025  Joachim Wiberg <troglobit@gmail.com>
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

#include <crypt.h>
#include <err.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifndef SULOGIN_USER
#define SULOGIN_USER "root"
#endif

/* getpwnam() cannot be used when statically linked */
static int get_passwd(struct passwd *pw)
{
	char *ptr, *tmp;
	char buf[256];
	FILE *fp;

	fp = fopen("/etc/passwd", "r");
	if (!fp)
		return 1;

	while ((tmp = fgets(buf, sizeof(buf), fp))) {
		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;

		pw->pw_name = strdup(ptr);
		if (!pw->pw_name || strcmp(pw->pw_name, SULOGIN_USER))
			continue;

		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;

		/* x : password in /etc/shadow, see passwd(5) */
		if (ptr[0] != 'x')
			pw->pw_passwd = strdup(ptr);

		break;			/* done, check it */
	}
	fclose(fp);

	if (!pw->pw_name)
		return 1;		/* no uid 0 here */
	if (pw->pw_passwd)
		return 0;		/* passwd in passwd */

	fp = fopen("/etc/shadow", "r");
	if (!fp) {
		if (pw->pw_name)
			free(pw->pw_name);
		return 1;
	}

	while ((tmp = fgets(buf, sizeof(buf), fp))) {
		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;

		if (!strcmp(tmp, pw->pw_name))
			continue;

		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;

		pw->pw_passwd = strdup(ptr);
		break;
	}
	fclose(fp);

	if (!pw->pw_passwd) {
		if (pw->pw_name)
			free(pw->pw_name);
		return 1;		/* nopass => nologin */
	}

	return 0;			/* passwd in shadow */
}

static void freepw(struct passwd *pw)
{
	if (pw->pw_passwd) {
		free(pw->pw_passwd);
		pw->pw_passwd = NULL;
	}
	free(pw->pw_name);
}

static int sh(void)
{
	/*
	 * Become session leader and set controlling TTY
	 * to enable Ctrl-C and job control in shell.
	 */
	setsid();
	ioctl(STDIN_FILENO, TIOCSCTTY, 1);
	setenv("PS1", "# ", 1);

	return execl(_PATH_BSHELL, "-sh", NULL);
}

int main(void)
{
	struct termios old, raw;
	struct passwd pw = { 0 };
	int rc = 1, ch;

	if (get_passwd(&pw))
		err(1, "reading credentials for UID 0");

	switch (pw.pw_passwd[0]) {
	case '*': case '!':	/* login disabled, allow */
		printf("The %s account is locked, allowing entry.\n", pw.pw_name);
		/* fallthrough */
	case 0:			/* no password at all? */
		printf("Press enter for maintenance (Ctrl-D to continue) ");
		fflush(stdout);
		ch = getchar();
		if (ch == 4 || ch == EOF) { /* Ctrl-D */
			rc = 0;
			goto done;
		}
		goto nopass;

	case '$': /* md5/shaN or similar, original DES not supported */
		break;
	default:
		break;
	}

	if (tcgetattr(1, &old) < 0)
		goto done;		/* failed reading tty */

	raw = old;
	raw.c_lflag &= ~ICANON;
	raw.c_lflag &= ~ECHO;
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(1, TCSANOW, &raw) < 0) {
                tcsetattr(1, TCSANOW, &old);
		goto done;		/* failed setting raw/pw mode */
	}

	while (1) {
		char pwd[256];		/* reasonable max? */
		size_t i = 0;
		char *passwd;

		printf("Give %s password for maintenance (Ctrl-D to continue): ", pw.pw_name);
		fflush(stdout);
		do {
			ch = getchar();
			if (ch == '\n' || ch == '\r')
				break;
			if (ch == 4 || ch == EOF) { /* Ctrl-D */
				rc = 0;
				goto exit;
			}

			pwd[i++] = ch;
		} while (i + 1 < sizeof(pwd));
		pwd[i] = 0;
		puts("");

		passwd = crypt(pwd, pw.pw_passwd);
		if (passwd && !strcmp(passwd, pw.pw_passwd)) {
			tcsetattr(1, TCSANOW, &old);	/* restore tty */
		nopass:
			freepw(&pw);

			/* tell initctl/reboot to trigger force mode */
			setenv("SULOGIN", "finit", 1);

			return sh();
		}
	}

exit:
	tcsetattr(1, TCSANOW, &old);	/* restore tty */
	puts("");
done:
	freepw(&pw);
	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
