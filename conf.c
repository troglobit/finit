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

#include <string.h>

#include "finit.h"
#include "svc.h"
#include "helpers.h"

/* Match one command. */
#define MATCH_CMD(l, c, x)					\
	(!strncmp(l, c, strlen(c)) && (x = (l) + strlen(c)))


static char *build_cmd(char *cmd, char *line, int len)
{
	int l;
	char *c;

	/* Trim leading whitespace */
	while (*line && (*line == ' ' || *line == '\t'))
		line++;

	if (!cmd) {
		cmd = malloc (strlen(line) + 1);
		if (!cmd) {
			_e("No memory left for '%s'", line);
			return NULL;
		}
		*cmd = 0;
	}
	c = cmd + strlen(cmd);
	for (l = 0; *line && *line != '#' && *line != '\t' && l < len; l++)
		*c++ = *line++;
	*c = 0;

	_d("cmd = %s", cmd);
	return cmd;
}

void parse_finit_conf(char *file)
{
	FILE *fp;
	char line[LINE_SIZE];
	char cmd[CMD_SIZE];

	username = strdup(DEFUSER);
	hostname = strdup(DEFHOST);
	rcsd     = strdup(FINIT_RCSD);

	if ((fp = fopen(file, "r")) != NULL) {
		char *x;

		_d("Parse %s ...", file);
		while (!feof(fp)) {
			if (!fgets(line, sizeof(line), fp))
				continue;
			chomp(line);

			_d("conf: %s", line);

			/* Skip comments. */
			if (MATCH_CMD(line, "#", x)) {
				continue;
			}
			/* Do this before mounting / read-write */
			if (MATCH_CMD(line, "check ", x)) {
				strcpy(cmd, "/sbin/fsck -C -a ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "Checking file system integrity on %s", x);
				continue;
			}
			if (MATCH_CMD(line, "user ", x)) {
				if (username) free(username);
				username = build_cmd(NULL, x, USERNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "host ", x)) {
				if (hostname) free(hostname);
				hostname = build_cmd(NULL, x, HOSTNAME_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "shutdown ", x)) {
				if (sdown) free(sdown);
				sdown = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "module ", x)) {
				strcpy(cmd, "/sbin/modprobe ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "   Loading module %s", x);
				continue;
			}
			if (MATCH_CMD(line, "mknod ", x)) {
				strcpy(cmd, "/bin/mknod ");
				build_cmd(cmd, x, CMD_SIZE);
				run_interactive(cmd, "   Creating device node %s", x);
				continue;
			}
			if (MATCH_CMD(line, "network ", x)) {
				if (network) free(network);
				network = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "runparts ", x)) {
				if (rcsd) free(rcsd);
				rcsd = build_cmd(NULL, x, CMD_SIZE);
				continue;
			}
			if (MATCH_CMD(line, "startx ", x)) {
				svc_register(x, username);
				continue;
			}
			if (MATCH_CMD(line, "service ", x)) {
				svc_register(x, NULL);
				continue;
			}
		}
		fclose(fp);
	}
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
