/* reboot, halt, poweroff, shutdown, suspend for finit
 *
 * Copyright (c) 2009-2016  Joachim Nilsson <troglobit@gmail.com>
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
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>

#ifndef RB_SW_SUSPEND /* Since Linux 2.5.18, but not in GLIBC until 2.19 */
#define RB_SW_SUSPEND	0xd000fce2
#endif

typedef enum {
	CMD_UNKNOWN = -1,
	CMD_REBOOT = 0,
	CMD_HALT,
	CMD_POWEROFF,
	CMD_SUSPEND,
} cmd_t;

static cmd_t cmd = CMD_UNKNOWN;
static char *msg = NULL;
extern char *__progname;

static void translate(void)
{
	if (cmd == CMD_UNKNOWN) {
		if (!strcmp(__progname, "halt") || !strcmp(__progname, "shutdown"))
			cmd = CMD_HALT;
		else if (!strcmp(__progname, "poweroff"))
			cmd = CMD_POWEROFF;
		else if (!strcmp(__progname, "suspend"))
			cmd = CMD_SUSPEND;
		else
			cmd = CMD_REBOOT;
	}

	switch (cmd) {
	case CMD_REBOOT:
		msg = "reboot";
		break;

	case CMD_HALT:
		msg = "halt";
		break;

	case CMD_POWEROFF:
		msg = "power-off";
		break;

	case CMD_SUSPEND:
		msg = "suspend";
		break;

	case CMD_UNKNOWN:
		cmd = CMD_REBOOT;
		msg = "reboot";
		break;
	}
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n\n"
		"Options:\n"
		"  -h, --help      This help text\n"
		"  -f, --force     Force unsafe %s now, do not contact the init system.\n"
		"      --halt      Halt system, regardless of how the command is called.\n"
		"  -p, --poweroff  Power-off system, regardless of how the command is called.\n"
		"      --reboot    Reboot system, regardless of how the command is called.\n"
		"\n", __progname, msg);

	return rc;
}

int main(int argc, char *argv[])
{
	int c, force = 0;
	struct option long_options[] = {
		{"help",     0, NULL, 'h'},
		{"force",    0, NULL, 'f'},
		{"halt",     0, NULL, 'H'},
		{"poweroff", 0, NULL, 'p'},
		{"reboot",   0, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};

	/* Initial command taken from program name */
	translate();

	while ((c = getopt_long(argc, argv, "h?fHpr", long_options, NULL)) != EOF) {
		switch(c) {
		case 'h':
		case '?':
			return usage(0);

		case 'f':
			force = 1;
			break;

		case 'H':
			cmd = CMD_HALT;
			break;

		case 'p':
			cmd = CMD_POWEROFF;
			break;

		case 'r':
			cmd = CMD_REBOOT;
			break;

		default:
			return usage(1);
		}
	}

	/* Check for any overrides */
	translate();

	if (force) {
		switch (cmd) {
		case CMD_REBOOT:
			c = reboot(RB_AUTOBOOT);
			break;

		case CMD_HALT:
			c = reboot(RB_HALT_SYSTEM);
			break;

		case CMD_POWEROFF:
			c = reboot(RB_POWER_OFF);
			break;

		case CMD_SUSPEND:
			c = reboot(RB_SW_SUSPEND);
			break;

		case CMD_UNKNOWN:
			errx(1, "Invalid command");
			break;
		}

		if (c)
			warn("Failed forced %s", msg);
		else
			return 0;
	}

	switch (cmd) {
	case CMD_REBOOT:
		c = kill(1, SIGTERM);
		break;

	case CMD_HALT:
		c = kill(1, SIGUSR1);
		break;

	case CMD_POWEROFF:
		c = kill(1, SIGUSR2);
		break;

	case CMD_SUSPEND:
	case CMD_UNKNOWN:
		errx(1, "Invalid command");
		break;
	}

	if (c)
		err(1, "Failed signalling init to %s", msg);
	else
		sleep(2);

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
