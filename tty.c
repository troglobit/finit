/* TTY handling in finit
 *
 * Copyright (c) 2013 Mattias Walström <lazzer@gmail.com>
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

#include <sys/inotify.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#include "queue.h"
#include "finit.h"
#include "helpers.h"
#include "lite.h"

typedef struct finit_tty_t {
	char *name;
	int baud;

	int pid; /* Runtime information*/
} finit_tty_t;

typedef struct node {
	LIST_ENTRY(node) link;
	finit_tty_t data;
} node_t;


/* We need to make room for the filename also, which is sent after inotify_event. */
#define EVENT_SIZE ((sizeof(struct inotify_event) + FILENAME_MAX))

LIST_HEAD(, node) node_list = LIST_HEAD_INITIALIZER();
int tty_add(char *tty, int baud)
{
	node_t *entry = malloc(sizeof(*entry));

	if (!entry)
		return errno = ENOMEM;

	entry->data.name = tty;
	entry->data.baud = baud;

	LIST_INSERT_HEAD(&node_list, entry, link);

	return 0;
}

static void tty_startstop_one(finit_tty_t *tty, uint32_t mask)
{
	int is_console = 0;
	char cmd[strlen(GETTY) + FILENAME_MAX];

	if (console && !strcmp(tty->name, console))
		is_console = 1;

	if (mask & IN_CREATE) {
		_d("Starting %s as %s", tty->name, is_console ? "console" : "TTY");
		snprintf(cmd, sizeof(cmd), GETTY, tty->name);
		tty->pid = run_getty(cmd, is_console);
	}

	/* Kill the spawned child, and recollect it. */
	if ((mask & IN_DELETE) && tty->pid) {
		_d("Stopping %s", tty->name);
		kill(tty->pid, SIGKILL);
		waitpid(tty->pid, NULL, 0);
		tty->pid = 0;
	}
}

static void tty_watcher(node_t *entry)
{
	int fd = inotify_init();

	prctl(PR_SET_NAME, "tty-watcher", 0, 0, 0);

	if (-1 == fd || inotify_add_watch(fd, "/dev", IN_CREATE | IN_DELETE) < 0) {
		fprintf(stderr, "finit: Failed starting TTY watcher: %s\n", strerror(errno));
		exit(1);
	}

	while (1) {
		int len = 0;
		char buf[EVENT_SIZE];
		struct inotify_event *notified = (struct inotify_event *)buf;

		while ((len = read(fd, buf, EVENT_SIZE))) {
			if (len == -1) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
			}

			LIST_FOREACH(entry, &node_list, link) {
				if (!strcmp(notified->name, entry->data.name))
					tty_startstop_one(&entry->data, notified->mask);
			}
		}
	}
}

void tty_start(void)
{
	node_t *entry;

	/* Start all TTYs that already exist in the system */
	LIST_FOREACH(entry, &node_list, link) {
		chdir("/dev");
		if (fexist (entry->data.name))
			tty_startstop_one(&entry->data, IN_CREATE);
	}

	/* Start a watcher to catch new TTYs that is discovered, e.g., USB */
	if (!fork())
		tty_watcher(entry);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
