/* Finit client for querying status and control services
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

#include <getopt.h>
#include <stdio.h>

#include "finit.h"
#include "helpers.h"
#include "service.h"

#include "libite/lite.h"


static char *runlevel_string(svc_t *svc)
{
	int i, pos = 1;;
	static char lvl[10] = "[]";

	for (i = 0; i < 10; i++) {
		if (ISSET(svc->runlevels, i)) {
			if (i == 0)
				lvl[pos++] = 'S';
			else
				lvl[pos++] = '0' + i;
		}
	}
	lvl[pos++] = ']';
	lvl[pos]   = 0;

	return lvl;
}

static int status(void)
{
	svc_t *svc;

	printf("Status   PID     Runlevels  Service               Description\n");
	printf("==============================================================================\n");
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		printf("%7s  %-6d  %-9s  %-20s  %s\n",
		       svc->pid ? "running" : "waiting", svc->pid,
		       runlevel_string(svc), svc->cmd, svc->desc);
	}

	return 0;
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [status | <1-6>]\n\n"
		"Options:\n"
		"  -d, --debug          Enable/Disable debug\n"
		"  -h, --help           This help text\n\n"
		"Commands:\n"
		"        status         Show status of services\n\n", __progname);

	return rc;
}

int client(int argc, char *argv[])
{
	int fd;
	int c;
	int result = -1;
	ssize_t len;
	struct {
		char  *cmd;
		int  (*cb)(void);
	} command[] = {
		{ "status", status },
		{ NULL, NULL }
	};
	struct option long_options[] = {
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = 0
	};

	while ((c = getopt_long(argc, argv, "h?d", long_options, NULL)) != EOF) {
		switch(c) {
		case 'd':
			rq.cmd = INIT_CMD_DEBUG;
			break;

		case 'h':
		case '?':
			return usage(0);
		default:
			return usage(1);
		}
	}

	if (!rq.cmd) {
		if (argc < 2) {
			fprintf(stderr, "Missing argument.\n");
			return usage(1);
		}

		for (c = 0; command[c].cmd; c++) {
			if (!strcmp(command[c].cmd, argv[1]))
				return command[c].cb();
		}

		rq.cmd = INIT_CMD_RUNLVL;
		rq.runlevel = (int)argv[1][0];
	}

	if (!fexist(FINIT_FIFO)) {
		fprintf(stderr, "/sbin/init does not support %s!\n", FINIT_FIFO);
		return 1;
	}

	fd = open(FINIT_FIFO, O_RDWR);
	if (-1 == fd) {
		perror("Failed opening " FINIT_FIFO);
		return 1;
	}

	len = write(fd, &rq, sizeof(rq));
	if (len == sizeof(rq)) {
		len = read(fd, &rq, sizeof(rq));
		if (len == sizeof(rq) && rq.cmd == INIT_CMD_ACK)
			result = 0;
	}

	close(fd);

	return result;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
