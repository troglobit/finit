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
	int  (*cb)(char *arg);
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

static int toggle_debug(char *UNUSED(arg))
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_DEBUG,
	};

	return do_send(&rq, sizeof(rq));
}

static int set_runlevel(char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RUNLVL,
		.runlevel = (int)arg[0],
	};

	return do_send(&rq, sizeof(rq));
}

static int do_svc(int cmd, char *jobid)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = cmd,
	};

	if (!jobid) {
		if (cmd == INIT_CMD_RELOAD_SVC) {
			rq.cmd = INIT_CMD_RELOAD;
			goto exit;
		}

		return 1;
	}
	strlcpy(rq.data, jobid, sizeof(rq.data));

exit:
	return do_send(&rq, sizeof(rq));
}

static int do_start  (char *arg) { return do_svc(INIT_CMD_START_SVC,   arg); }
static int do_stop   (char *arg) { return do_svc(INIT_CMD_STOP_SVC,    arg); }
static int do_reload (char *arg) { return do_svc(INIT_CMD_RELOAD_SVC,  arg); }
static int do_restart(char *arg) { return do_svc(INIT_CMD_RESTART_SVC, arg); }

static int show_version(char *UNUSED(arg))
{
	puts("v" VERSION);
	return 0;
}

/* In verbose mode we skip the header and each service description.
 * This in favor of having all info on one line so a machine can more
 * easily parse it. */
static int show_status(char *UNUSED(arg))
{
	svc_t *svc;

	if (!verbose) {
		printf("#       Status   PID     Runlevels  Service               Description\n");
		printf("====================================================================================\n");
	}

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		char jobid[10];

		if (svc_is_unique(svc))
			snprintf(jobid, sizeof(jobid), "%d", svc->job);
		else
			snprintf(jobid, sizeof(jobid), "%d:%d", svc->job, svc->id);

		if (verbose) {
			int i;
			char args[80] = "";

			for (i = 1; i < MAX_NUM_SVC_ARGS; i++) {
				strlcat(args, svc->args[i], sizeof(args));
				strlcat(args, " ", sizeof(args));
			}
			printf("%-6s  %7s  %-6d  %-9s  %s %s\n", jobid, svc_status(svc), svc->pid,
			       runlevel_string(svc->runlevels), svc->cmd, args);
		} else {
			printf("%-6s  %7s  %-6d  %-9s  %-20s  %s\n", jobid, svc_status(svc), svc->pid,
			       runlevel_string(svc->runlevels), svc->cmd, svc->desc);
		}
	}

	return 0;
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [<COMMAND> | <q | 1-6>]\n\n"
		"Options:\n"
		"  -d, --debug           Toggle Finit debug\n"
		"  -v, --verbose         Verbose output\n"
		"  -h, --help            This help text\n\n"
		"Commands:\n"
		"        debug           Toggle Finit debug\n"
		"        q | reload      Reload *.conf in /etc/finit.d/\n"
		"        runlevel <1-6>  Set new runlevel\n"
		"        status          Show status of services\n"
		"        start   <JOB#>  Start stopped service\n"
		"        stop    <JOB#>  Stop running service\n"
		"        restart <JOB#>  Restart (stop/start) running service\n"
		"        reload  <JOB#>  Reload (SIGHUP) running service\n"
		"        version         Show Finit version\n\n", __progname);

	return rc;
}

int client(int argc, char *argv[])
{
	int c;
	command_t command[] = {
		{ "debug",    toggle_debug },
		{ "q",        do_reload    }, /* SysV init compat. */
		{ "reload",   do_reload    },
		{ "runlevel", set_runlevel },
		{ "status",   show_status  },
		{ "start",    do_start     },
		{ "stop",     do_stop      },
		{ "restart",  do_restart   },
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
			toggle_debug(NULL);
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
		char *cmd = argv[optind++];
		char *job = optind < argc ? argv[optind] : NULL;

		/* Compat: 'init <1-9>' */
		if (strlen(cmd) == 1 && isdigit((int)cmd[0]))
			return set_runlevel(cmd);

		for (c = 0; command[c].cmd; c++) {
			if (!strcmp(command[c].cmd, cmd))
				return command[c].cb(job);
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
