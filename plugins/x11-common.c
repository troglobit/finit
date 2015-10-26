/* Console setup (for X)
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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../finit.h"
#include "../helpers.h"
#include "../plugin.h"
#include "libite/lite.h"

static void setup(void *UNUSED(arg))
{
#ifdef PAM_CONSOLE
	int fd;
#endif
	char line[LINE_SIZE];

	makedir("/var/run/console", 01777);
	snprintf(line, sizeof(line), "/var/run/console/%s", username);
	touch(line);

#ifdef PAM_CONSOLE
	if ((fd = open("/var/run/console/console.lock", O_CREAT|O_WRONLY|O_TRUNC, 0644)) >= 0) {
		write(fd, username, strlen(username));
		close(fd);
		run("/sbin/pam_console_apply");
	}
#endif

	makedir("/tmp/.X11-unix", 01777);
	makedir("/tmp/.ICE-unix", 01777);
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
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
