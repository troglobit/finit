/* poor man's sulogin, may be statically linked
 *
 * Copyright (c) 2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <crypt.h>
#include <err.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

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
		if (!strstr(buf, ":0:0:"))
			continue;	/* not uid 0 */

		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;
		pw->pw_name = strdup(ptr);

		ptr = strsep(&tmp, ":");
		if (!ptr)
			break;

		if (strlen(ptr) > 1)
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

int main(void)
{
	struct termios old, raw;
	struct passwd pw = { 0 };

	if (get_passwd(&pw))
		err(1, "reading credentials for UID 0");

	/* is login disabled, not much we can do */
	if (strlen(pw.pw_passwd) == 1)
		goto done;		/* likely one of: "x!*" */

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

		printf("Give %s password for maintenance: ", pw.pw_name);
		do {
			int ch;

			ch = getchar();
			if (ch == EOF || ch == '\n' || ch == '\r')
				break;
			if (ch == 4)	/* Ctrl-D */
				goto exit;

			pwd[i++] = ch;
		} while (i + 1 < sizeof(pwd));
		pwd[i] = 0;
		puts("");

		passwd = crypt(pwd, pw.pw_passwd);
		if (passwd && !strcmp(passwd, pw.pw_passwd)) {
			char arg0[strlen(_PATH_BSHELL) + 2];
			char *args[2] = {
				arg0,
				NULL
			};

			tcsetattr(1, TCSANOW, &old);	/* restore tty */
			freepw(&pw);
			snprintf(arg0, sizeof(arg0), "-%s", _PATH_BSHELL);
			setenv("PS1", "# ", 1);

			return execv(_PATH_BSHELL, args);
		}
	}

exit:
	tcsetattr(1, TCSANOW, &old);	/* restore tty */
	puts("");
done:
	freepw(&pw);
	return 1;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
