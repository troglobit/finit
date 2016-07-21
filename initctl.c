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
#include <glob.h>
#include <paths.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <lite/lite.h>

#include "config.h"
#include "finit.h"
#include "cond.h"
#include "helpers.h"
#include "service.h"

#define _PATH_COND _PATH_VARRUN "finit/cond/"

typedef struct {
	char  *cmd;
	int  (*cb)(char *arg);
} command_t;

int debug    = 0;
int verbose  = 0;
int silent   = 1;		/* For helpers.c */
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

static int runlevel_get(void)
{
	int sd, result = 255;
	struct init_request rq;
	struct sockaddr_un sun = {
		.sun_family = AF_UNIX,
		.sun_path   = INIT_SOCKET,
	};

	memset(&rq, 0, sizeof(rq));
	rq.cmd = INIT_CMD_GET_RUNLEVEL;
	rq.magic = INIT_MAGIC;

	sd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (-1 == sd)
		return -1;

	if (connect(sd, (struct sockaddr*)&sun, sizeof(sun)) == -1)
		goto error;

	if (write(sd, &rq, sizeof(rq)) != sizeof(rq))
		goto error;

	if (read(sd, &rq, sizeof(rq)) != sizeof(rq))
		goto error;

	result = rq.runlevel;
	goto exit;
error:
	perror("Failed communicating with finit");
	result = -1;
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

static int do_runlevel(char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RUNLVL,
		.runlevel = (int)arg[0],
	};

	/* Not compatible with the SysV runlevel(8) command, it prints
	 * "PREVLEVEL RUNLEVEL", we just print the current runlevel. */
	if (!rq.runlevel) {
		printf("%d\n", runlevel_get());
		return 0;
	}

	return do_send(&rq, sizeof(rq));
}

static int do_svc(int cmd, char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = cmd,
	};

	strlcpy(rq.data, arg, sizeof(rq.data));

	return do_send(&rq, sizeof(rq));
}

static int do_emit   (char *arg) { return do_svc(INIT_CMD_EMIT,        arg); }
static int do_reload (char *arg) { return do_svc(INIT_CMD_RELOAD,      arg); }
static int do_start  (char *arg) { return do_svc(INIT_CMD_START_SVC,   arg); }
static int do_stop   (char *arg) { return do_svc(INIT_CMD_STOP_SVC,    arg); }
static int do_restart(char *arg) { return do_svc(INIT_CMD_RESTART_SVC, arg); }

static void show_cond_one(const char *_conds)
{
	static char conds[MAX_ARG_LEN];
	char *cond;

	strlcpy(conds, _conds, sizeof(conds));

	putchar('<');

	for (cond = strtok(conds, ","); cond; cond = strtok(NULL, ",")) {
		if (cond != conds)
			putchar(',');

		switch (cond_get(cond)) {
		case COND_ON:
			printf("+%s", cond);
			break;

		case COND_FLUX:
			printf("\e[1m~%s\e[0m", cond);
			break;

		case COND_OFF:
			printf("\e[1m-%s\e[0m", cond);
			break;
		}
	}

	putchar('>');
}

static int do_cond_magic(char op, char *cond)
{
	cond[0] = op;
	return do_emit(cond);
}

static int do_cond_set  (char *cond) { return do_cond_magic('+', cond); }
static int do_cond_clear(char *cond) { return do_cond_magic('-', cond); }
static int do_cond_flux (char *cond) { return do_cond_magic('~', cond); }

static int do_cond_show(char *UNUSED(arg))
{
	svc_t *svc;
	enum cond_state cond;

	if (verbose) {
		glob_t gl;

		printf("Asserted conditions (taken from %s)\n", _PATH_COND);
		printf("===============================================================================\n");

		if (!glob(_PATH_COND "*/*/*", 0, NULL, &gl)) {
			size_t i;

			for (i = 0; i < gl.gl_pathc; i++) {
				char *cond, *name = gl.gl_pathv[i];
				struct stat st;

				if (stat(name, &st) || S_ISDIR(st.st_mode))
					continue;

				cond = name + strlen(_PATH_COND);
				printf("\t%s\n", cond);
			}
			globfree(&gl);
		}
		puts("");
	}

	printf("PID     Service               Status  Condition (+ on, ~ flux, - off)\n");
	printf("===============================================================================\n");

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!svc->cond[0])
			continue;

		cond = cond_get_agg(svc->cond);

		printf("%-6d  %-20.20s  ", svc->pid, svc->cmd);

		if (cond == COND_ON)
			printf("%-6.6s  ", condstr(cond));
		else
			printf("\e[1m%-6.6s\e[0m  ", condstr(cond));

		show_cond_one(svc->cond);
		puts("");
	}

	return 0;
}

static int do_cond(char *cmd)
{
	int c;
	char *arg;
	command_t command[] = {
		{ "set",     do_cond_set   },
		{ "clear",   do_cond_clear },
		{ "flux",    do_cond_flux  },
		{ "show",    do_cond_show  },
		{ NULL, NULL }
	};

	arg = strpbrk(cmd, " ");
	if (arg) {
		*arg = 0;

		for (c = 0; command[c].cmd; c++) {
			if (!strcmp(command[c].cmd, cmd))
				return command[c].cb(arg);
		}
	}

	return do_cond_show(NULL);
}

static int show_version(char *UNUSED(arg))
{
	puts("v" PACKAGE_VERSION);
	return 0;
}

/* In verbose mode we skip the header and each service description.
 * This in favor of having all info on one line so a machine can more
 * easily parse it. */
static int show_status(char *arg)
{
	svc_t *svc;

	/* Fetch UTMP runlevel, needed for svc_status() call below */
	runlevel = runlevel_get();

	if (arg && arg[0]) {
		int id = 1;
		char *ptr, *next;

		ptr = strchr(arg, ':');
		if (ptr) {
			*ptr++ = 0;
			next = strchr(ptr, ' ');
			if (next)
				*next = 0;
			id = atonum(ptr);
		}

		if (isdigit(arg[0]))
			svc = svc_find_by_jobid(atonum(arg), id);
		else
			svc = svc_find_by_nameid(arg, id);

		if (!svc)
			return 1;

		printf("%s\n", svc_status(svc));
		return 0;
	}

	if (!verbose) {
		printf("#      Status   PID     Runlevels   Service               Description\n");
		printf("===============================================================================\n");
	}

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		char jobid[10], args[512] = "", *lvls;

		if (svc_is_unique(svc))
			snprintf(jobid, sizeof(jobid), "%d", svc->job);
		else
			snprintf(jobid, sizeof(jobid), "%d:%d", svc->job, svc->id);

		printf("%-5s  %7s  ", jobid, svc_status(svc));
		if (svc_is_inetd(svc))
			printf("inetd   ");
		else
			printf("%-6d  ", svc->pid);

		lvls = runlevel_string(svc->runlevels);
		if (strchr(lvls, '\e'))
			printf("%-18.18s  ", lvls);
		else
			printf("%-10.10s  ", lvls);

		if (!verbose) {
			char *name = strrchr(svc->cmd, '/');

			if (!name)
				name = svc->cmd;
			else
				name++;

			printf("%-21.21s  %s\n", name, svc->desc);
			continue;
		}

#ifndef INETD_DISABLED
		if (svc_is_inetd(svc)) {
			char *info;
			struct init_request rq = {
				.magic = INIT_MAGIC,
				.cmd = INIT_CMD_QUERY_INETD,
			};

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
		}
		else
#endif /* !INETD_DISABLED */
		{
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
		"  -d, --debug               Debug initctl (client)\n"
		"  -v, --verbose             Verbose output\n"
		"  -h, --help                This help text\n\n"
		"Commands:\n"
		"  debug                     Toggle Finit (daemon) debug\n"
		"  help                      This help text\n"
		"  reload                    Reload *.conf in /etc/finit.d and activate changes\n"
		"  runlevel [0-9]            Show or set runlevel: 0 halt, 6 reboot\n"
		"  status | show             Show status of services\n"
		"  cond     set   <COND>     Set (assert) condition     => +COND\n"
		"  cond     clear <COND>     Clear (deassert) condition => -COND\n"
		"  cond     flux  <COND>     Emulate flux condition     => ~COND\n"
		"  cond     show             Show condition status\n"
		"  start    <JOB|NAME>[:ID]  Start service by job# or name, with optional ID\n"
		"  stop     <JOB|NAME>[:ID]  Stop/Pause a running service by job# or name\n"
		"  restart  <JOB|NAME>[:ID]  Restart (stop/start) service by job# or name\n"
		"  version                   Show Finit version\n\n", __progname);

	return rc;
}

int main(int argc, char *argv[])
{
	int c;
	command_t command[] = {
		{ "debug",    toggle_debug },
		{ "reload",   do_reload    },
		{ "runlevel", do_runlevel  },
		{ "status",   show_status  },
		{ "show",     show_status  }, /* Convenience alias */
		{ "cond",     do_cond      },
		{ "start",    do_start     },
		{ "stop",     do_stop      },
		{ "restart",  do_restart   },
		{ "version",  show_version },
		{ NULL, NULL }
	};
	struct option long_options[] = {
		{"help",    0, NULL, 'h'},
		{"debug",   0, NULL, 'd'},
		{"verbose", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "dh?v", long_options, NULL)) != EOF) {
		switch(c) {
		case 'h':
		case '?':
			return usage(0);

		case 'd':
			debug = 1;
			break;

		case 'v':
			verbose = 1;
			break;
		}
	}
	silent = !verbose;

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
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
