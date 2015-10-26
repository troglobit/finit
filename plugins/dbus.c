/* Setup and start system message bus, D-Bus
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

#include <sys/types.h>

#include "../finit.h"
#include "../config.h"
#include "../helpers.h"
#include "../plugin.h"
#include "libite/lite.h"

static void setup(void *UNUSED(arg))
{
#ifdef HAVE_DBUS
	_d("Starting D-Bus ...");
	makedir("/var/run/dbus", 0755);
	makedir("/var/lock/subsys/messagebus", 0755);
	run("dbus-uuidgen --ensure");
	erase("/var/run/dbus/pid");
	run_interactive("dbus-daemon --system", "Starting D-Bus");
#endif
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_NETWORK_UP] = {
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
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
