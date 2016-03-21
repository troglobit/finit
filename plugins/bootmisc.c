/* Setup necessary files for UTMP, tracking logins
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

#include <string.h>
#include <utmp.h>

#include "../config.h"
#include "../finit.h"
#include "../helpers.h"
#include "../plugin.h"
#include "libite/lite.h"

/*
 * Setup standard FHS 2.3 structure in /var, and
 * write runlevel to UTMP, needed by, e.g., printerdrake.
 */
static void setup(void *UNUSED(arg))
{
#ifdef RUNLEVEL
	struct utmp entry;
#endif

	_d("Setting up FHS structure in /var ...");
	makedir("/var/cache",      0755);
	makedir("/var/games",      0755);
	makedir("/var/lib",        0755);
	makedir("/var/lib/misc",   0755);
	makedir("/var/lib/alarm",  0755);
	makedir("/var/lib/urandom",0755);
	makedir("/var/lock",       0755);
	makedir("/var/log",        0755);
	makedir("/var/mail",       0755);
	makedir("/var/opt",        0755);
	makedir("/var/run",        0755);
	makedir("/var/spool",      0755);
	makedir("/var/spool/cron", 0755);
	makedir("/var/tmp",        0755);
	makedir("/var/empty",      0755);

	_d("Setting up necessary UTMP files ...");
	touch("/var/run/utmp");
	chown("/var/run/utmp", 0, getgroup("utmp"));

#ifdef RUNLEVEL
	memset(&entry, 0, sizeof(struct utmp));
	entry.ut_type = RUN_LVL;
	entry.ut_pid = '0' + RUNLEVEL;
	setutent();
	pututline(&entry);
	endutent();
#endif

#ifdef TOUCH_ETC_NETWORK_RUN_IFSTATE
	touch("/etc/network/run/ifstate");
#else
	erase("/etc/network/run/ifstate");
#endif

	_d("Setting up misc files ...");
	makedir("/var/run/lldpd",  0755); /* Needed by lldpd */
	makedir("/var/run/pluto",  0755); /* Needed by Openswan */
	makedir("/var/run/quagga", 0755); /* Needed by Quagga */
	makedir("/var/log/quagga", 0755); /* Needed by Quagga */
	makedir("/var/run/sshd", 01755);  /* OpenSSH  */
	makefifo("/dev/xconsole", 0640);  /* sysklogd */
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
