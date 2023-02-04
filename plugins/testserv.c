/* Test plugin, only used for `make check`, not for public use
 *
 * Copyright (c) 2023  Joachim Wiberg <troglobit@gmail.com>
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

#include <sys/types.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "finit.h"
#include "config.h"
#include "helpers.h"
#include "plugin.h"
#include "service.h"
#include "conf.h"

#define SERV_DAEMON "serv"
#define SERV_ARGS   "-n -p"
#define SERV_DESC   "Test serv daemon"

static void setup(void *arg)
{
	char line[256];
	mode_t prev;
	char *cmd;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	cmd = which(SERV_DAEMON);
	if (!cmd) {
		dbg("Skipping plugin, %s is not installed.", SERV_DAEMON);
		return;
	}

	/* Clean up from any previous pre-bootstrap run */
	remove("/run/serv.pid");

	prev = umask(0);
	mksubsys("/var/run/serv", 0755, "root", "root");
	mksubsys("/var/run/lock/subsys", 0755, "root", "root");
	mksubsys("/var/lib/serv", 0755, "root", "root");
	mksubsys("/tmp/serv", 0755, "root", "root");
	umask(prev);

	snprintf(line, sizeof(line), "[S123456789] pid:!/run/serv.pid cgroup.system %s %s -- %s",
		 cmd, SERV_ARGS, SERV_DESC);

	if (service_register(SVC_TYPE_SERVICE, line, global_rlimit, NULL))
		err(1, "Failed registering %s", SERV_DAEMON);

	free(cmd);
}

static plugin_t plugin = {
	.name                  = __FILE__,
	.hook[HOOK_SVC_PLUGIN] = { .cb  = setup },
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
