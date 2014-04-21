/* Parser for finit.conf
 *
 * Copyright (c) 2012  Joachim Nilsson <troglobit@gmail.com>
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
#include <string.h>

#include "finit.h"
#include "svc.h"
#include "tty.h"
#include "lite.h"
#include "helpers.h"

#define MATCH_CMD(l, c, x) \
	(!strncmp(l, c, strlen(c)) && (x = (l) + strlen(c)))

static char *strip_line(char *line)
{
	char *ptr;

	/* Trim leading whitespace */
	while (*line && isblank(*line))
		line++;

	/* Strip any comment at end of line */
	ptr = line;
	while (*ptr && *ptr != '#')
		ptr++;
	*ptr = 0;

	return line;
}

/* Convert optional "[!123456789S]" string into a bitmask */
int parse_runlevels(char *runlevels)
{
	int i, not = 0, bitmask = 0;

	if (!runlevels)
		runlevels = "[234]";
	i = 1;
	while (i) {
		int level;
		char lvl = runlevels[i++];

		if (']' == lvl || 0 == lvl)
			break;
		if ('!' == lvl) {
			not = 1;
			bitmask = 0x3FE;
			continue;
		}

		if ('s' == lvl || 'S' == lvl)
			lvl = '0';

		level = lvl - '0';
		if (level > 9 || level < 0)
			continue;

		if (not)
			CLRBIT(bitmask, level);
		else
			SETBIT(bitmask, level);
	}

	return bitmask;
}

static int parse_conf(char *file)
{
	FILE *fp;
	char line[LINE_SIZE] = "";
	char cmd[CMD_SIZE];
	char *x;
	const char *err = NULL;

	fp = fopen(file, "r");
	if (!fp)
		return 1;

	/*
	 * If not standard finit.conf, then we want to show just the base name
	 * Loading configuration ............. vs
	 * Loading services configuration ....
	 */
	if (!string_match (file, FINIT_CONF)) {
		/* Remove leading path */
		x = strrchr(file, '/');
		if (!x) x = file;
		else	x++;

		/* Remove ending .conf */
		strlcpy(line, x, sizeof(line));
		x = strstr(line, ".conf");
		if (x) *x = 0;

		/* Add empty space. */
		strcat(line, " ");
	}

	print(0, "Loading %sconfiguration", line);
	while (!feof(fp)) {
		if (!fgets(line, sizeof(line), fp))
			continue;
		chomp(line);
		_d("conf: %s", line);

		/* Skip comments. */
		if (MATCH_CMD(line, "#", x))
			continue;

		/* Do this before mounting / read-write
		 * XXX: Move to plugin which checks /etc/fstab instead */
		if (MATCH_CMD(line, "check ", x)) {
			char *dev = strip_line(x);

			strcpy(cmd, "/sbin/fsck -C -a ");
			strlcat(cmd, dev, sizeof(cmd));
			run_interactive(cmd, "Checking file system %s", dev);

			continue;
		}

		if (MATCH_CMD(line, "user ", x)) {
			if (username) free(username);
			username = strdup(strip_line(x));
			continue;
		}
		if (MATCH_CMD(line, "host ", x)) {
			if (hostname) free(hostname);
			hostname = strdup(strip_line(x));
			continue;
		}

		if (MATCH_CMD(line, "module ", x)) {
			char *mod = strip_line(x);

			strcpy(cmd, "/sbin/modprobe ");
			strlcat(cmd, mod, sizeof(cmd));
			run_interactive(cmd, "Loading kernel module %s", mod);

			continue;
		}
		if (MATCH_CMD(line, "mknod ", x)) {
			char *dev = strip_line(x);

			strcpy(cmd, "/bin/mknod ");
			strlcat(cmd, dev, sizeof(cmd));
			run_interactive(cmd, "Creating device node %s", dev);

			continue;
		}

		if (MATCH_CMD(line, "network ", x)) {
			if (network) free(network);
			network = strdup(strip_line(x));
			continue;
		}
		/* This is the directory from which executable scripts and
		 * any finit include files are read, default /etc/finit.d/ */
		if (MATCH_CMD(line, "runparts ", x)) {
			if (rcsd) free(rcsd);
			rcsd = strdup(strip_line(x));
			continue;
		}
		/* Parse any include file, use rcsd if absolute path not given */
		if (MATCH_CMD(line, "include ", x)) {
			char *file = strip_line(x);

			strlcpy(cmd, file, sizeof(cmd));
			if (!fexist(cmd)) {
				/* ... try /etc/finit.d/ as prefix */
				snprintf(cmd, sizeof(cmd), "%s/%s", rcsd, file);
				if (!fexist(cmd))
					continue;
			}

			parse_conf(cmd);
			continue;
		}
		if (MATCH_CMD(line, "startx ", x)) {
			svc_register(SVC_CMD_SERVICE, strip_line(x), username);
			continue;
		}
		if (MATCH_CMD(line, "shutdown ", x)) {
			if (sdown) free(sdown);
			sdown = strdup(strip_line(x));
			continue;
		}

		/* The desired runlevel to start when leaving
		 * bootstrap (S).  Finit supports 1-9, but most
		 * systems only use 1-6, where 6 is reserved for
		 * reboot */
		if (MATCH_CMD(line, "runlevel ", x)) {
			char *token = strip_line(x);

			cfglevel = strtonum(token, 1, 9, &err);
			if (err)
				cfglevel = RUNLEVEL;
			if (cfglevel < 1 || cfglevel > 9 || cfglevel == 6)
				cfglevel = 2; /* Fallback */
			continue;
		}

		/* Monitored daemon, will be respawned on exit, as
		 * long as the (optional) service callback returns
		 * non-zero */
		if (MATCH_CMD(line, "service ", x)) {
			svc_register(SVC_CMD_SERVICE, x, NULL);
			continue;
		}

		/* One-shot task, will not be respawned. Only runs if
		 * the (optional) service callback returns true */
		if (MATCH_CMD(line, "task ", x)) {
			svc_register(SVC_CMD_TASK, x, NULL);
			continue;
		}

		/* Like task but waits for completion, useful w/ [S] */
		if (MATCH_CMD(line, "run ", x)) {
			svc_register(SVC_CMD_RUN, x, NULL);
			continue;
		}

		if (MATCH_CMD(line, "console ", x)) {
			if (console) free(console);
			console = strdup(strip_line(x));
			continue;
		}
		if (MATCH_CMD(line, "tty ", x)) {
			tty_register(strip_line(x));
			continue;
		}
	}

	fclose(fp);

	return 0;
}

int parse_finit_conf(char *file)
{
	int result;

	username = strdup(DEFUSER);
	hostname = strdup(DEFHOST);
	rcsd     = strdup(FINIT_RCSD);

	result = parse_conf(file);
	if (!tty_num())
		tty_register(FALLBACK_SHELL);

	return result;
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
