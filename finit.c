/* Finit - Fast /sbin/init replacement w/ I/O, hook & service plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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

#include <sys/mount.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>		/* umask(), mkdir() */

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "plugin.h"
#include "service.h"
#include "sig.h"
#include "tty.h"
#include "libite/lite.h"
#include "inetd.h"

int   debug     = 0;
int   verbose   = VERBOSE_MODE;
int   runlevel  = 0;		/* Bootstrap */
int   cfglevel  = RUNLEVEL;	/* Fallback if no configured runlevel */
int   prevlevel = 0;		/* HALT */
char *sdown     = NULL;
char *network   = NULL;
char *username  = NULL;
char *hostname  = NULL;
char *rcsd      = FINIT_RCSD;
char *runparts  = NULL;
char *console   = NULL;

uev_ctx_t *ctx  = NULL;		/* Main loop context */


int main(int argc, char* argv[])
{
	uev_ctx_t loop;

	if (getpid() != 1)
		return client(argc, argv);

	/*
	 * Hello world.
	 */
	banner();

	/*
	 * Initial setup of signals, ignore all until we're up.
	 */
	sig_init();

	/*
	 * Initalize event context.
	 */
	uev_init(&loop);
	ctx = &loop;

	/*
	 * Mount base file system, kernel is assumed to run devtmpfs for /dev
	 */
	chdir("/");
	umask(0);
	mount("none", "/proc", "proc", 0, NULL);
	mount("none", "/proc/bus/usb", "usbfs", 0, NULL);
	mount("none", "/sys", "sysfs", 0, NULL);
	makedir("/dev/pts", 0755);
	makedir("/dev/shm", 0755);
	mount("none", "/dev/pts", "devpts", 0, "gid=5,mode=620");
	mount("none", "/dev/shm", "tmpfs", 0, NULL);
	umask(022);

	/*
	 * Parse kernel parameters
	 */
	parse_kernel_cmdline();

	/*
	 * Populate /dev and prepare for runtime events from kernel.
	 */
	run_interactive(SETUP_DEVFS, "Populating device tree");

	/*
	 * Load plugins first, finit.conf may contain references to
	 * features implemented by plugins.
	 */
	plugin_load_all(&loop, PLUGIN_PATH);

	/*
	 * Parse configuration file
	 */
	parse_finit_conf(FINIT_CONF);

	/* Set hostname as soon as possible, for syslog et al. */
	set_hostname(&hostname);

	/*
	 * Mount filesystems
	 */
#ifdef REMOUNT_ROOTFS_RW
	run("/bin/mount -n -o remount,rw /");
#endif
#ifdef SYSROOT
	run(SYSROOT, "/", NULL, MS_MOVE, NULL);
#endif

	_d("Root FS up, calling hooks ...");
	plugin_run_hooks(HOOK_ROOTFS_UP);

	umask(0);
	run("/bin/mount -na");
	run("/sbin/swapon -ea");
	umask(0022);

	/* Cleanup stale files, if any still linger on. */
	run_interactive("rm -rf /tmp/* /var/run/* /var/lock/*", "Cleanup temporary directories");

	/* Base FS up, enable standard SysV init signals */
	sig_setup(&loop);

	_d("Base FS up, calling hooks ...");
	plugin_run_hooks(HOOK_BASEFS_UP);

	/*
	 * Start all bootstrap tasks, no network available!
	 */
	service_bootstrap();

	/*
	 * Network stuff
	 */

	/* Setup kernel specific settings, e.g. allow broadcast ping, etc. */
	run("/sbin/sysctl -e -p /etc/sysctl.conf >/dev/null");

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);
	if (network)
		run_interactive(network, "Starting networking: %s", network);
	umask(022);

	/* Hooks that rely on loopback, or basic networking being up. */
	plugin_run_hooks(HOOK_NETWORK_UP);

	/*
	 * Load .conf files from /etc/finit.d and start all
	 * tasks/services in the configured runlevel
	 */
	parse_finit_d(rcsd);
	service_runlevel(cfglevel);

	_d("Running svc up hooks ...");
	plugin_run_hooks(HOOK_SVC_UP);

	/*
	 * Run startup scripts in the runparts directory, if any.
	 */
	if (runparts && fisdir(runparts)) {
		_d("Running startup scripts in %s ...", runparts);
		run_parts(runparts, NULL);
	}

	/* Hooks that should run at the very end */
	plugin_run_hooks(HOOK_SYSTEM_UP);

	/* Start TTYs */
	tty_runlevel(runlevel);

	/*
	 * Enter main loop to monior /dev/initctl and services
	 */
	return uev_run(&loop, 0);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
