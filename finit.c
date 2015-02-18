/* Finit - Extremely fast /sbin/init replacement w/ I/O, hook & service plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2014  Joachim Nilsson <troglobit@gmail.com>
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
#include <getopt.h>

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "plugin.h"
#include "svc.h"
#include "sig.h"
#include "tty.h"
#include "lite.h"
#include "inetd.h"

int   debug     = 0;
int   verbose   = 1;
int   runlevel  = 0;		/* Bootstrap */
int   cfglevel  = RUNLEVEL;	/* Fallback if no configured runlevel */
int   prevlevel = 0;		/* HALT */
char *sdown     = NULL;
char *network   = NULL;
char *username  = NULL;
char *hostname  = NULL;
char *rcsd      = NULL;
char *console   = NULL;

static void parse_kernel_cmdline(void)
{
	FILE *fp;
	char line[LINE_SIZE];

	if ((fp = fopen("/proc/cmdline", "r")) != NULL) {
		fgets(line, sizeof(line), fp);
		_d("Kernel command line: %s", line);

		if (strstr(line, "finit_debug") || strstr(line, "--debug"))
			debug = 1;

		if (!debug && strstr(line, "quiet"))
			verbose = 0;

		fclose(fp);
	}
}

static int usage(int rc)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [RUNLEVEL]\n\n"
		"  -d, --debug          Enable/Disable debug\n"
		"  -h, --help           This help text\n\n", __progname);

	return rc;
}

static int client(int argc, char *argv[])
{
	int fd;
	int c;
	struct option long_options[] = {
		{"debug", 0, NULL, 'd'},
		{"help", 0, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = 0
	};

	while ((c = getopt_long (argc, argv, "h?d", long_options, NULL)) != EOF) {
		switch(c) {
		case 'd':
			rq.cmd = INIT_CMD_DEBUG;
			break;

		case 'h':
		case '?':
			return usage(0);
		default:
			return usage(1);
		}
	}

	if (!rq.cmd) {
		if (argc < 2) {
			fprintf(stderr, "Missing runlevel.\n");
			return 1;
		}

		rq.cmd = INIT_CMD_RUNLVL;
		rq.runlevel = (int)argv[1][0];
	}

	if (!fexist(FINIT_FIFO)) {
		fprintf(stderr, "/sbin/init does not support %s!\n", FINIT_FIFO);
		return 1;
	}

	fd = open(FINIT_FIFO, O_WRONLY);
	if (-1 == fd) {
		perror("Failed opening " FINIT_FIFO);
		return 1;
	}
	write(fd, &rq, sizeof(rq));
	close(fd);

	return 0;
}

static void banner(void)
{
	delline();
	echo("Finit version " VERSION " (" WHOAMI ") " __DATE__ " " __TIME__);
}

int main(int argc, char* argv[])
{
	uev_ctx_t ctx;

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
	uev_init(&ctx);

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
	print_result(plugin_load_all(&ctx, PLUGIN_PATH));

	/*
	 * Parse configuration file
	 */
	parse_finit_conf(FINIT_CONF);

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
	sig_setup(&ctx);

	_d("Base FS up, calling hooks ...");
	plugin_run_hooks(HOOK_BASEFS_UP);

	/*
	 * Start all bootstrap tasks, no network available!
	 */
	svc_bootstrap();

	/*
	 * Network stuff
	 */

	/* Setup kernel specific settings, e.g. allow broadcast ping, etc. */
	run("/sbin/sysctl -e -p /etc/sysctl.conf >/dev/null");

	/* Set initial hostname. */
	set_hostname(&hostname);

	ifconfig("lo", "127.0.0.1", "255.0.0.0", 1);
	if (network)
		run_interactive(network, "Starting networking: %s", network);
	umask(022);

	/* Hooks that rely on loopback, or basic networking being up. */
	plugin_run_hooks(HOOK_NETWORK_UP);

	/*
	 * Start all tasks/services in the configured runlevel
	 */
	svc_runlevel(cfglevel);

	/* Start inetd services */
	inetd_runlevel(&ctx, runlevel);

	_d("Running svc up hooks ...");
	plugin_run_hooks(HOOK_SVC_UP);

	/*
	 * Run startup scripts in /etc/finit.d/, if any.
	 */
	if (rcsd && fisdir(rcsd)) {
		_d("Running startup scripts in %s ...", rcsd);
		run_parts(rcsd, NULL);
	}

	/* Hooks that should run at the very end */
	plugin_run_hooks(HOOK_SYSTEM_UP);

	/* Start TTYs */
	tty_runlevel(runlevel);

	/*
	 * Enter main loop to monior /dev/initctl and services
	 */
	return uev_run(&ctx, 0);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
