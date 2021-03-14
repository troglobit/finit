/* Console setup (for X)
 *
 * Copyright (c) 2012-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <lite/lite.h>

#include "finit.h"
#include "helpers.h"
#include "plugin.h"

static void setup(void *arg)
{
	const char *username = "nobody";
	char line[LINE_SIZE];
	mode_t prev;
#ifdef PAM_CONSOLE
	int fd;
#endif

	prev = umask(0);

	makedir("/var/run/console", 01777);
	snprintf(line, sizeof(line), "/var/run/console/%s", username);
	touch(line);

#ifdef PAM_CONSOLE
	if ((fd = open("/var/run/console/console.lock", O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
		write(fd, username, strlen(username));
		close(fd);
		run("pam_console_apply");
	}
#endif

	makedir("/tmp/.X11-unix", 01777);
	makedir("/tmp/.ICE-unix", 01777);

	if (whichp("restorecon"))
		run("restorecon /tmp/.ICE-unix /tmp/.X11-unix");

	umask(prev);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_NETWORK_UP] = {
		.cb  = setup
	},
	.depends = { "bootmisc", }
};

PLUGIN_INIT(plugin_init)
{
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
