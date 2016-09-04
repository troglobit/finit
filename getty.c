/* Initialize and serve a login terminal
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

#include <err.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <termios.h>

#include "finit.h"
#include "helpers.h"
#include "sig.h"
#include "utmp-api.h"

#ifndef _PATH_LOGIN
#define _PATH_LOGIN  "/bin/login"
#endif

#define print(s) (void)write(STDOUT_FILENO, s, strlen(s))


/*
 * Read one character from stdin.
 */
static int readch(char *tty)
{
	int st;
	char ch1;

	st = read(STDIN_FILENO, &ch1, 1);
	if (st == 0) {
		print("\n");
		_exit(0);
	}

	if (st < 0)
		errx(1, "getty: %s: read error.", tty);

	return ch1 & 0xFF;
}

static void stty(int fd, speed_t speed)
{
	struct termios term;

	tcdrain(fd);
	if (tcgetattr(fd, &term))
		return;

	cfsetispeed(&term, speed);
	cfsetospeed(&term, speed);
	tcsetattr(fd, TCSAFLUSH, &term);

	tcflush(fd, TCIOFLUSH);

	/* Clear screen -- disabled until we decide on config to disable/enable */
	//print("\033[r\033[H\033[J");
}


/*
 * Parse and display a line from /etc/issue
 */
static void do_parse(char *line, struct utsname *uts, char *tty)
{
	char *s, *s0;

	s0 = line;
	for (s = line; *s != 0; s++) {
		if (*s == '\\') {
			(void)write(1, s0, s - s0);
			s0 = s + 2;
			switch (*++s) {
			case 'l':
				print(tty);
				break;
			case 'm':
				print(uts->machine);
				break;
			case 'n':
				print(uts->nodename);
				break;
#ifdef _GNU_SOURCE
			case 'o':
				print(uts->domainname);
				break;
#endif
			case 'r':
				print(uts->release);
				break;
			case 's':
				print(uts->sysname);
				break;
			case 'v':
				print(uts->version);
				break;
			case 0:
				goto leave;
			default:
				s0 = s - 1;
			}
		}
	}

leave:
	(void)write(1, s0, s - s0);
}

/*
 * Parse and display /etc/issue
 */
static void do_issue(char *tty)
{
	FILE *fp;
	char buf[BUFSIZ] = "Welcome to \\s \\v \\n \\l\n\n";
	struct utsname uts;

	/*
	 * Get data about this machine.
	 */
	uname(&uts);

	print("\n");
	fp = fopen("/etc/issue", "r");
	if (fp) {
		while (fgets(buf, sizeof(buf), fp))
			do_parse(buf, &uts, tty);

		fclose(fp);
	} else {
		do_parse(buf, &uts, tty);
	}

	do_parse("\\n login: ", &uts, tty);
}

/*
 * Handle the process of a GETTY.
 */
static void do_getty(char *tty, char *name, size_t len)
{
	int ch;
	char *np;

	/*
	 * Clean up tty name.
	 */
	if (!strncmp(tty, _PATH_DEV, strlen(_PATH_DEV)))
		tty += 5;

	/*
	 * Display prompt.
	 */
	ch = ' ';
	*name = '\0';
	while (ch != '\n') {
		do_issue(tty);

		np = name;
		while ((ch = readch(tty)) != '\n') {
			if (np < name + len)
				*np++ = ch;
		}

		*np = '\0';
		if (*name == '\0')
			ch = ' ';	/* blank line typed! */
	}

	name[len - 1] = 0;
}

/* Execute the login(1) command with the current
 * username as its argument. It will reply to the
 * calling user by typing "Password: "...
 */
static int do_login(char *name)
{
	struct stat st;

	execl(_PATH_LOGIN, _PATH_LOGIN, name, NULL);

	/*
	 * Failed to exec login, should be impossible.  Try a starting a
	 * fallback shell instead.
	 *
	 * Note: Add /etc/securetty handling.
	 */
	warnx("Failed exec %s, attempting fallback to %s ...", _PATH_LOGIN, _PATH_BSHELL);
	if (fstat(0, &st) == 0 && S_ISCHR(st.st_mode))
		execl(_PATH_BSHELL, _PATH_BSHELL, NULL);

	return 1;	/* We shouldn't get here ... */
}

static speed_t do_parse_speed(char *baud)
{
	char *ptr;
	size_t i;
	unsigned long val;
	struct { unsigned long val; speed_t speed; } v2s[] = {
		{       0, B0       },
		{      50, B50      },
		{      75, B75      },
		{     110, B110     },
		{     134, B134     },
		{     150, B150     },
		{     200, B200     },
		{     300, B300     },
		{     600, B600     },
		{    1200, B1200    },
		{    1800, B1800    },
		{    2400, B2400    },
		{    4800, B4800    },
		{    9600, B9600    },
		{   19200, B19200   },
		{   38400, B38400   },
		{   57600, B57600   },
		{  115200, B115200  },
		{  230400, B230400  },
		{  460800, B460800  },
		{  500000, B500000  },
		{  576000, B576000  },
		{  921600, B921600  },
		{ 1000000, B1000000 },
		{ 1152000, B1152000 },
		{ 1500000, B1500000 },
		{ 2000000, B2000000 },
		{ 2500000, B2500000 },
		{ 3000000, B3000000 },
		{ 3500000, B3500000 },
		{ 4000000, B4000000 },
	};

	errno = 0;
	val = strtoul(baud, &ptr, 10);
	if (errno || ptr == baud)
		return B0;

	for (i = 0; i < sizeof(v2s) / sizeof(v2s[0]); i++) {
		if (v2s[i].val == val)
			return v2s[i].speed;
	}

	return B0;
}

int getty(char *tty, char *baud, char *UNUSED(term), char *user)
{
	int fd;
	char name[30];
	speed_t speed = B38400;
	struct sigaction sa;

	if (baud) {
		speed = do_parse_speed(baud);
		if (speed == B0) {
			logit(LOG_CRIT, "TTY %s: Invalid speed %s", tty, baud);
			return 1;
		}
	}

	/* Detach from initial controlling TTY */
	vhangup();

	fd = open(tty, O_RDWR);
	if (fd < 0)
		err(1, "Failed opening %s", tty);

	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);

	if (ioctl(STDIN_FILENO, TIOCSCTTY, 1) < 0)
		warn("Failed TIOCSCTTY");

	/*
	 * Don't let QUIT dump core.
	 */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = _exit;
	sigaction(SIGQUIT, &sa, NULL);

	stty(fd, speed);
	utmp_set_login(tty, NULL);

	if (!user)
		do_getty(tty, name, sizeof(name));
	else
		strlcpy(name, user, sizeof(name));

	return do_login(name);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
