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
	char cmd[strlen(GETTY)+FILENAME_MAX];

	if (mask & IN_CREATE) {
		_d("Starting %s as %s", tty->name, (strcmp(tty->name, console) == 0) ? "console" : "TTY");
		snprintf(cmd, sizeof(cmd), GETTY, tty->name);
		tty->pid = run_getty(cmd, strcmp(tty->name, console) == 0);
	}

	/* Kill the spawned child, and recollect it. */
	if ((mask & IN_DELETE) && tty->pid) {
		int status = 0;
		_d("Stopping %s", tty->name);
		kill(tty->pid, SIGKILL);
		waitpid(tty->pid, &status, 0);
		tty->pid = 0;
	}
}

void tty_start(void)
{
	int   fd = -1;
	node_t *entry;

	/* Start all TTYs that already exist in the system. */
	LIST_FOREACH(entry, &node_list, link) {
		chdir("/dev");
		if (fexist (entry->data.name))
			tty_startstop_one(&entry->data, IN_CREATE);
	}

	/* Start a inotify watcher to catch new TTYs that is discovered (USB for example) */
	if (!fork()) {
		fd = inotify_init();

		prctl(PR_SET_NAME, "tty_watcher", 0, 0, 0);
		if (inotify_add_watch(fd, "/dev", IN_CREATE | IN_DELETE) < 0) {
			fprintf(stderr, "Failed to add watcher, errno %s\n", strerror(errno));
			return;
		}

		while (1) {
			int len = 0;;
			char buf[EVENT_SIZE];
			struct inotify_event *notified;

			while ((len = read(fd, buf, EVENT_SIZE))) {
				if (len == -1) {
					if (errno == EAGAIN || errno == EINTR)
						continue;
				}
				notified = (struct inotify_event *) &buf[0];
				LIST_FOREACH(entry, &node_list, link) {
					if (strcmp(notified->name, entry->data.name) == 0) {
						tty_startstop_one(&entry->data, notified->mask);
					}
				}
			}
		}
		exit(0);
	}

}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
