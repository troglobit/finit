/* Heuristically find and initialize a suitable hotplug daemon
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
#include <stdio.h>

#include "config.h"
#include "conf.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "service.h"

static void setup(void *arg)
{
	char cmd[256];
	char *path;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	/*
	 * Populate /dev and prepare for runtime events from kernel.
	 * Prefer udev if mdev is also available on the system.
	 */
	path = which("udevd");
	if (!path)
		path = which("/lib/systemd/systemd-udevd");
	if (path) {
		/* Register udevd as a monitored service */
		snprintf(cmd, sizeof(cmd), "[S12345789] cgroup.system pid:udevd name:udevd log %s "
			 "-- Device event managing daemon", path);
		if (service_register(SVC_TYPE_SERVICE, cmd, global_rlimit, NULL)) {
			err(1, "Failed registering %s", path);
		} else {
			snprintf(cmd, sizeof(cmd), "cgroup.init :1 [S] <pid/udevd> log "
				 "udevadm trigger -c add -t devices "
				 "-- Requesting device events");
			service_register(SVC_TYPE_RUN, cmd, global_rlimit, NULL);

			snprintf(cmd, sizeof(cmd), "cgroup.init :2 [S] <pid/udevd> log "
				 "udevadm trigger -c add -t subsystems "
				 "-- Requesting subsystem events");
			service_register(SVC_TYPE_RUN, cmd, global_rlimit, NULL);
		}

		free(path);

		/* Debian has this little script to copy generated
		 * rules while the system was read-only. TODO: When
		 * this functionality was hardcoded in finit.c, this
		 * call was made in crank_worker(). Now that
		 * filesystems are mounted earlier we should be able
		 * to make this call directly after the triggers have
		 * run, but this has not been tested AT ALL. */
		if (fexist("/lib/udev/udev-finish"))
			run_interactive("/lib/udev/udev-finish", "Finalizing udev");
	} else {
		path = which("mdev");
		if (path) {
			/* Embedded Linux systems usually have BusyBox mdev */
			if (debug)
				touch("/dev/mdev.log");

			snprintf(cmd, sizeof(cmd), "%s -s", path);
			free(path);

			run_interactive(cmd, "Populating device tree");
		}
	}
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = {
		.cb  = setup
	},
	.depends = { "bootmisc", "modprobe" },
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
