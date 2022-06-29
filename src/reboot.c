/* reboot, halt, poweroff, shutdown, suspend for finit
 *
 * Copyright (c) 2009-2022  Joachim Wiberg <troglobit@gmail.com>
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
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

typedef enum {
	CMD_UNKNOWN = -1,
	CMD_REBOOT = 0,
	CMD_HALT,
	CMD_POWEROFF,
	CMD_SUSPEND,
} cmd_t;

static cmd_t cmd = CMD_UNKNOWN;
static char *msg = NULL;

/* initctl API */
extern int do_reboot  (char *arg);
extern int do_halt    (char *arg);
extern int do_poweroff(char *arg);
extern int do_suspend (char *arg);


static void transform(char *nm)
{
	if (!nm)
		nm = prognm;

	if (cmd == CMD_UNKNOWN) {
		if (!strcmp(nm, "halt"))
			cmd = CMD_HALT;
		else if (!strcmp(nm, "poweroff") || !strcmp(nm, "shutdown"))
			cmd = CMD_POWEROFF;
		else if (!strcmp(nm, "suspend"))
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

/*
 * fallback in case of initctl API failure
 */
static void do_kill(int signo, char *msg)
{
	if (kill(1, signo))
		err(1, "Failed signalling init to %s", msg);
	do_sleep(5);
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n\n"
		"Options:\n"
		"  -?, --help      This help text\n"
		"  -f, --force     Force unsafe %s now, do not contact the init system.\n"
		"      --halt      Halt system, regardless of how the command is called.\n"
		"  -h              Halt or power off after shutdown.\n"
		"  -P, --poweroff  Power-off system, regardless of how the command is called.\n"
		"  -r, --reboot    Reboot system, regardless of how the command is called.\n"
		"\n", prognm, msg);

	return rc;
}

int reboot_main(int argc, char *argv[])
{
	struct option long_options[] = {
		{"help",     0, NULL, '?'},
		{"force",    0, NULL, 'f'},
		{"halt",     0, NULL, 'H'},
		{"poweroff", 0, NULL, 'p'},
		{"reboot",   0, NULL, 'r'},
		{NULL, 0, NULL, 0}
	};
	int c, force = 0;

	/* Initial command taken from program name */
	transform(prognm);

	while ((c = getopt_long(argc, argv, "h?fHPpr", long_options, NULL)) != EOF) {
		switch(c) {
		case '?':
			return usage(0);

		case 'f':
			force = 1;
			break;

		case 'H':
			cmd = CMD_HALT;
			break;

		case 'h':
		case 'P':
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
	transform(NULL);

	/* Check if we're in sulogin shell */
	if (getenv("SULOGIN"))
		force = 1;

	if (force) {
		sync();

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
			c = suspend();
			if (c && errno == EINVAL)
				errx(1, "Kernel does not support suspend to RAM.");
			break;

		case CMD_UNKNOWN:
			errx(1, "Invalid command");
			break;
		}

		if (c)
			err(1, "Failed forced %s", msg);

		return 0;
	}

	switch (cmd) {
	case CMD_REBOOT:
		if (do_reboot(NULL))
			do_kill(SIGTERM, msg);
		break;

	case CMD_HALT:
		if (do_halt(NULL))
			do_kill(SIGUSR2, msg);
		break;

	case CMD_POWEROFF:
		if (do_poweroff(NULL))
			do_kill(SIGUSR2, msg);
		break;

	case CMD_SUSPEND:
		/*
		 * Only initctl supports suspend, we avoid adding
		 * another signal to finit for compat reasons.
		 */
		do_suspend(NULL);
		break;


	case CMD_UNKNOWN:
		errx(1, "Invalid command");
		break;
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
