/* Simple telinit client
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
#include "libite/lite.h"

static int do_send(int cmd, int runlevel)
{
	int fd;
	int result = 255;
	ssize_t len;
	struct init_request rq = {
		.magic    = INIT_MAGIC,
		.cmd      = cmd,
		.runlevel = runlevel,
	};

	if (!fexist(FINIT_FIFO)) {
		fprintf(stderr, "%s does not support %s!\n", __progname, FINIT_FIFO);
		return 1;
	}

	fd = open(FINIT_FIFO, O_RDWR);
	if (-1 == fd) {
		perror("Failed opening " FINIT_FIFO);
		return 1;
	}

	len = write(fd, &rq, sizeof(rq));
	if (sizeof(rq) == len)
		result = 0;
	close(fd);

	return result;
}

/* telinit q | telinit <0-9> */
static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [q | Q | 0-9]\n\n"
		"Options:\n"
		"  -h, --help            This help text\n\n"
		"  -V, --version         Show Finit version\n\n"
		"Commands:\n"
		"  q | Q                 Reload *.conf in /etc/finit.d/, like SIGHUP\n"
		"  0 - 9                 Change runlevel: 0 halt, 6 reboot\n\n", __progname);

	return rc;
}

int client(int argc, char *argv[])
{
	int c;
	struct option long_options[] = {
		{"help",    0, NULL, 'h'},
		{"version", 0, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};

	verbose = 0;
	while ((c = getopt_long(argc, argv, "h?v", long_options, NULL)) != EOF) {
		switch(c) {
		case 'v':
			return puts("v" VERSION) == EOF;

		case 'h':
		case '?':
			return usage(0);
		}
	}

	if (optind < argc) {
		int req = (int)argv[optind][0];

		/* Compat: 'init <0-9>' */
		if (isdigit(req))
			return do_send(INIT_CMD_RUNLVL, req);

		if (req == 'q' || req == 'Q')
			return do_send(INIT_CMD_RELOAD, 0);
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
