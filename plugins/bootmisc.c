/* Setup necessary system files for, e.g. UTMP (tracking logins)
 *
 * Copyright (c) 2012-2016  Joachim Nilsson <troglobit@gmail.com>
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

#include <string.h>
#include <utmp.h>
#include <lite/lite.h>

#include "../config.h"
#include "../finit.h"
#include "../helpers.h"
#include "../plugin.h"

/*
 * Setup standard FHS 2.3 structure in /var, and write runlevel to UTMP
 */
static void setup(void *UNUSED(arg))
{
	struct utmp entry;

	_d("Setting up FHS structure in /var ...");
	makedir("/var/cache",      0755);
	makedir("/var/games",      0755);
	makedir("/var/lib",        0755);
	makedir("/var/lib/misc",   0755);
	makedir("/var/lib/alarm",  0755);
	makedir("/var/lib/urandom",0755);
	if (fisdir("/run")) {
		_d("System with new /run tmpfs ...");
		makedir("/run/lock",       1777);
		symlink("/run/lock", "/var/lock");
		symlink("/run",      "/var/run");
		symlink("/dev/shm",  "/run/shm");
	} else {
		makedir("/var/lock",       1777);
		makedir("/var/run",        0755);
	}
	makedir("/var/lock/subsys",0755); /* Needed by D-Bus plugin */
	makedir("/var/log",        0755);
	makedir("/var/mail",       0755);
	makedir("/var/opt",        0755);
	makedir("/var/spool",      0755);
	makedir("/var/spool/cron", 0755);
	makedir("/var/tmp",        0755);
	makedir("/var/empty",      0755);

	_d("Setting up necessary UTMP files ...");
	touch("/var/run/utmp");
	chmod("/var/run/utmp", 0664);
	chown("/var/run/utmp", 0, getgroup("utmp"));
	touch("/var/log/wtmp");
	chmod("/var/log/wtmp", 0664);
	chown("/var/log/wtmp", 0, getgroup("utmp"));
	touch("/var/log/lastlog");
	chmod("/var/log/lastlog", 0664);
	chown("/var/log/lastlog", 0, getgroup("utmp"));

	_d("Setting initial runlevel 'S', bootstrap ...");
	runlevel_set(0, 0);	/* Bootstrap 'S' */

#ifdef TOUCH_ETC_NETWORK_RUN_IFSTATE
	touch("/etc/network/run/ifstate");
#else
	erase("/etc/network/run/ifstate");
#endif

	_d("Setting up misc files ...");
	makedir("/var/run/network",0755); /* Needed by Debian/Ubuntu ifupdown */
	makedir("/var/run/lldpd",  0755); /* Needed by lldpd */
	makedir("/var/run/pluto",  0755); /* Needed by Openswan */
	makedir("/var/run/quagga", 0755); /* Needed by Quagga */
	makedir("/var/log/quagga", 0755); /* Needed by Quagga */
	makedir("/var/run/sshd",  01755); /* OpenSSH  */
	makefifo("/dev/xconsole",  0640); /* sysklogd */
	chown("/dev/xconsole", 0, getgroup("tty"));
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = {
		.cb  = setup
	},
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
