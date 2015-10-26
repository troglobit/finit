/* Optional TTY Watcher, used to catch new TTYs that are discovered, e.g., USB
 *
 * Copyright (c) 2013  Mattias Walström <lazzer@gmail.com>
 * Copyright (c) 2013  Joachim Nilsson <troglobit@gmail.com>
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

#include <errno.h>
#include <fcntl.h>		/* O_RDONLY et al */
#include <unistd.h>		/* read() */
#include <sys/inotify.h>
#include <sys/stat.h>

#include "../finit.h"
#include "../helpers.h"
#include "../plugin.h"
#include "../tty.h"
#include "libite/lite.h"

static void watcher(void *arg, int fd, int events);

static plugin_t plugin = {
	.name = __FILE__,
	.io = {
		.cb    = watcher,
		.flags = PLUGIN_IO_READ,
	},
};

static void setup(void)
{
	plugin.io.fd = inotify_init1(IN_NONBLOCK);
	if (-1 == plugin.io.fd || inotify_add_watch(plugin.io.fd, "/dev", IN_CREATE | IN_DELETE) < 0)
		_e("Failed starting TTY watcher: %s", strerror(errno));
}

static void watcher(void *UNUSED(arg), int fd, int UNUSED(events))
{
	int len = 0;
	char buf[EVENT_SIZE], name[42];
	tty_node_t *entry;
	struct inotify_event *notified = (struct inotify_event *)buf;

	while ((len = read(fd, buf, sizeof(buf)))) {
		if (-1 == len) {
			if (errno == EINTR)
				continue;
			break;
		}

		snprintf(name, sizeof(name), "/dev/%s", notified->name);
		entry = tty_find(name);
		if (entry && tty_enabled(&entry->data, runlevel)) {
			if (notified->mask & IN_CREATE)
				tty_start(&entry->data);
			else if (entry->data.pid)
				tty_stop(&entry->data);
		}
	}
}

PLUGIN_INIT(plugin_init)
{
	setup();
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
