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

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>

#include "finit.h"
#include "helpers.h"
#include "service.h"

#include "libite/lite.h"

typedef struct {
	char  *cmd;
	int  (*cb)(void);
} command_t;


static int do_send(struct init_request *rq, ssize_t len)
{
	int fd;
	int result = -1;
	ssize_t num;

	if (!fexist(FINIT_FIFO)) {
		fprintf(stderr, "%s does not support %s!\n", __progname, FINIT_FIFO);
		return 1;
	}

	fd = open(FINIT_FIFO, O_RDWR);
	if (-1 == fd) {
		perror("Failed opening " FINIT_FIFO);
		return 1;
	}

	num = write(fd, rq, len);
	if (num == len) {
		num = read(fd, rq, len);
		if (num == len && rq->cmd == INIT_CMD_ACK)
			result = 0;
	}

	close(fd);

	return result;
}

static int toggle_debug(void)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_DEBUG,
	};

	return do_send(&rq, sizeof(rq));
}

static int reload_conf(void)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RELOAD,
	};

	return do_send(&rq, sizeof(rq));
}

static int set_runlevel(int lvl)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RUNLVL,
		.runlevel = lvl,
	};

	return do_send(&rq, sizeof(rq));
}

int show_version(void)
{
	puts("v" VERSION);
	return 0;
}

static char *svc_status(svc_t *svc)
{
	if (svc->pid)
		return "running";

	if (svc_is_inetd(svc))
		return "inetd";

	return "waiting";
}

static int show_status(void)
{
	svc_t *svc;

	printf("Status   PID     Runlevels  Service               Description\n");
	printf("==============================================================================\n");
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		printf("%7s  %-6d  %-9s  %-20s  %s\n", svc_status(svc), svc->pid,
		       runlevel_string(svc->runlevels), svc->cmd, svc->desc);
		if (verbose) {
			int i;
			char args[80] = "";

			for (i = 1; i < MAX_NUM_SVC_ARGS; i++) {
				strlcat(args, svc->args[i], sizeof(args));
				strlcat(args, " ", sizeof(args));
			}
			printf("                            %s\n", args);
		}
	}

	return 0;
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [<COMMAND> | <q | 1-6>]\n\n"
		"Options:\n"
		"  -d, --debug          Toggle Finit debug\n"
		"  -v, --verbose        Verbose output\n"
		"  -h, --help           This help text\n\n"
		"Commands:\n"
		"        debug          Toggle Finit debug\n"
		"        reload         Reload *.conf in /etc/finit.d/\n"
		"        status         Show status of services\n"
		"        version        Show Finit version\n\n", __progname);

	return rc;
}

int client(int argc, char *argv[])
{
	int c;
	command_t command[] = {
		{ "debug",    toggle_debug },
		{ "q",        reload_conf  }, /* SysV init compat. */
		{ "reload",   reload_conf  },
//		{ "runlevel", set_runlevel },
		{ "status",   show_status  },
		{ "version",  show_version },
		{ NULL, NULL }
	};
	struct option long_options[] = {
		{"debug",   0, NULL, 'd'},
		{"help",    0, NULL, 'h'},
		{"verbose", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	verbose = 0;
	while ((c = getopt_long(argc, argv, "h?dv", long_options, NULL)) != EOF) {
		switch(c) {
		case 'd':
			toggle_debug();
			break;

		case 'h':
		case '?':
			return usage(0);

		case 'v':
			verbose = 1;
			break;
		}
	}

	if (optind < argc) {
		char *cmd = argv[optind];

		/* Compat: 'init <1-9>' */
		if (strlen(cmd) == 1 && isdigit((int)cmd[0]))
			return set_runlevel((int)cmd[0]);

		for (c = 0; command[c].cmd; c++) {
			if (!strcmp(command[c].cmd, cmd))
				return command[c].cb();
		}
	}

	return usage(1);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
