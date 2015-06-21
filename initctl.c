/* New client tool, complements old /dev/initctl API and telinit tool
 *
 * Copyright (c) 2015  Joachim Nilsson <troglobit@gmail.com>
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
#include <sys/socket.h>
#include <sys/un.h>

#include "finit.h"
#include "helpers.h"
#include "service.h"

#include "libite/lite.h"

typedef struct {
	char  *cmd;
	int  (*cb)(char *arg);
} command_t;

int debug    = 0;
int verbose  = 0;
int runlevel = 0;

static int do_send(struct init_request *rq, ssize_t len)
{
	int sd, result = 255;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sd)
		return -1;

	if (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1)
		goto error;

	if (write(sd, rq, len) != len)
		goto error;

	if (read(sd, rq, len) != len)
		goto error;

	result = 0;
	goto exit;
error:
	perror("Failed communicating with finit");
exit:
	close(sd);
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

static int do_svc(int cmd, char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = cmd,
	};

	if (!arg) {
		if (cmd == INIT_CMD_RELOAD_SVC) {
			rq.cmd = INIT_CMD_RELOAD;
			goto exit;
		}

		return 1;
	}
	strlcpy(rq.data, arg, sizeof(rq.data));

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

	/* Fetch UTMP runlevel, needed for svc_status() call below */
	runlevel = runlevel_get();

	if (!verbose) {
		printf("#      Status   PID     Runlevels  Service               Description\n");
		printf("====================================================================================\n");
	}

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		int  inetd = svc_is_inetd(svc);
		char jobid[10], args[80] = "";
		struct init_request rq = {
			.magic = INIT_MAGIC,
			.cmd = INIT_CMD_QUERY_INETD,
		};

		if (svc_is_unique(svc))
			snprintf(jobid, sizeof(jobid), "%d", svc->job);
		else
			snprintf(jobid, sizeof(jobid), "%d:%d", svc->job, svc->id);

		printf("%-5s  %7s  %-6d  %-9s  ", jobid, svc_status(svc), svc->pid, runlevel_string(svc->runlevels));
		if (!verbose) {
			printf("%-20s  %s\n", svc->cmd, svc->desc);
			continue;
		}

		if (inetd) {
			char *info;

			snprintf(rq.data, sizeof(rq.data), "%s", jobid);
			if (do_send(&rq, sizeof(rq))) {
				snprintf(args, sizeof(args), "Unknown inetd");
				info = args;
			} else {
				info = rq.data;
				if (strcmp("internal", svc->cmd)) {
					char *ptr = strchr(info, ' ');
					if (ptr)
						info = ptr + 1;
				}
			}

			printf("%s %s\n", svc->cmd, info);
		} else {
			int i;

			for (i = 1; i < MAX_NUM_SVC_ARGS; i++) {
				strlcat(args, svc->args[i], sizeof(args));
				strlcat(args, " ", sizeof(args));
			}

			printf("%s %s\n", svc->cmd, args);
		}
	}

	return 0;
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] <COMMAND>\n\n"
		"Options:\n"
		"  -v, --verbose         Verbose output\n"
		"  -h, --help            This help text\n\n"
		"Commands:\n"
		"  debug                 Toggle Finit debug\n"
		"  reload                Reload *.conf in /etc/finit.d/\n"
		"  runlevel <0-9>        Change runlevel: 0 halt, 6 reboot\n"
		"  status                Show status of services\n"
		"  start    <JOB|NAME>   Start stopped job or service\n"
		"  stop     <JOB|NAME>   Stop running job or service\n"
		"  restart  <JOB|NAME>   Restart (stop/start) job or service\n"
		"  reload   <JOB|NAME>   Reload (SIGHUP) job or service\n"
		"  version               Show Finit version\n\n", __progname);

	return rc;
}

int main(int argc, char *argv[])
{
	int c;
	command_t command[] = {
		{ "debug",    toggle_debug },
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
		{"help",    0, NULL, 'h'},
		{"verbose", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	verbose = 0;
	while ((c = getopt_long(argc, argv, "h?v", long_options, NULL)) != EOF) {
		switch(c) {
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
		char  arg[120] = "";

		while (optind < argc) {
			strlcat(arg, argv[optind++], sizeof(arg));
			strlcat(arg, " ", sizeof(arg));
		}

		for (c = 0; command[c].cmd; c++) {
			if (!strcmp(command[c].cmd, cmd))
				return command[c].cb(arg);
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
