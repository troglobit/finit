/* Setup and start system message bus, D-Bus
 *
 * Copyright (c) 2012-2022  Joachim Wiberg <troglobit@gmail.com>
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

#define DBUS_DAEMON "dbus-daemon"
#define DBUS_ARGS   "--nofork --system --syslog-only"
#define DBUS_DESC   "D-Bus message bus daemon"

#ifndef DBUS_DAEMONUSER
#define DBUS_DAEMONUSER "messagebus"
#endif

#ifndef DBUS_DAEMONGROUP
#define DBUS_DAEMONGROUP "messagebus"
#endif

#ifndef DBUS_DAEMONPIDFILE
#define DBUS_DAEMONPIDFILE "/var/run/dbus/pid"
#endif

static void setup(void *arg)
{
	char line[256];
	mode_t prev;
	char *cmd;

	if (rescue) {
		_d("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	cmd = which(DBUS_DAEMON);
	if (!cmd) {
		_d("Skipping plugin, %s is not installed.", DBUS_DAEMON);
		return;
	}

	prev =umask(0);

  _d("Creating D-Bus Required Directories ...");
  mksubsys("/var/run/dbus", 0755, DBUS_DAEMONUSER, DBUS_DAEMONGROUP);
  mksubsys("/var/run/lock/subsys", 0755, DBUS_DAEMONUSER, DBUS_DAEMONGROUP);
  mksubsys("/var/lib/dbus", 0755, DBUS_DAEMONUSER, DBUS_DAEMONGROUP);

  /* Generate machine id for dbus */
	if (whichp("dbus-uuidgen"))
		run_interactive("dbus-uuidgen --ensure", "Creating machine UUID for D-Bus");

	/* Clean up from any previous pre-bootstrap run */
	remove(DBUS_DAEMONPIDFILE);

	/* Register service with Finit */
	snprintf(line, sizeof(line), "[S12345789] cgroup.system pid:!%s @%s:%s %s %s -- %s",
		 DBUS_DAEMONPIDFILE, DBUS_DAEMONUSER, DBUS_DAEMONGROUP, cmd, DBUS_ARGS, DBUS_DESC);
	if (service_register(SVC_TYPE_SERVICE, line, global_rlimit, NULL))
		_pe("Failed registering %s", DBUS_DAEMON);
	free(cmd);

	umask(prev);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = {
		.cb  = setup
	},
	.depends = { "bootmisc", },
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
