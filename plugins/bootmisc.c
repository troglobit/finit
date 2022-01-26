/* Setup necessary system files for, e.g. UTMP (tracking logins)
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

#include <ftw.h>
#include <mntent.h>
#include <string.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "config.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "util.h"
#include "utmp-api.h"

static int is_tmpfs(char *path)
{
	int tmpfs = 0;
	FILE *fp;
	char *dir;
	struct mntent *mnt;

	/* If path is a symlink, check what it resolves to */
	dir = realpath(path, NULL);
	if (!dir)
		return 0;	/* Outlook not so good */

	fp = setmntent("/proc/mounts", "r");
	if (!fp) {
		free(dir);
		return 0;	/* Dunno, maybe not */
	}

	while ((mnt = getmntent(fp))) {
		if (!strcmp(dir, mnt->mnt_dir))
			break;
	}

	if (mnt && !strcmp("tmpfs", mnt->mnt_type))
		tmpfs = 1;

	endmntent(fp);
	free(dir);

	return tmpfs;
}

static int bootclean(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftw)
{
	if (ftw->level == 0)
		return 1;

	_d("Removing %s ...", fpath);
	(void)remove(fpath);

	return 0;
}

/*
 * Cleanup stale files from previous boot, if any still linger on.
 * Some systems, e.g. Alpine Linux, still have a persistent /run and
 * /tmp, i.e. not tmpfs.
 *
 * We can safely skip tmpfs, nothing to clean there.
 */
static void clean(void *arg)
{
	char *dir[] = {
		"/tmp/",
		"/var/run/",
		"/var/lock/",
		NULL
	};

	for (int i = 0; dir[i]; i++) {
		if (is_tmpfs(dir[i]))
			continue;

		nftw(dir[i], bootclean, 20, FTW_DEPTH);
	}
}

static void ln(const char *target, const char *linkpath)
{
	if (symlink(target, linkpath) && errno != EEXIST)
		_pe("Failed creating %s -> %s symlink", target, linkpath);
}

/*
 * Setup standard FHS 2.3 structure in /var, and write runlevel to UTMP
 */
static void setup(void *arg)
{
	mode_t prev;

	prev = umask(0);

	_d("Setting up FHS structure in /var ...");
	makedir("/var/cache",      0755);
	makedir("/var/db",         0755); /* _PATH_VARDB on some systems */
	makedir("/var/games",      0755);
	makedir("/var/lib",        0755);
	makedir("/var/lib/misc",   0755); /* _PATH_VARDB on some systems */
	makedir("/var/lib/alarm",  0755);
	makedir("/var/lib/urandom",0755);
	if (fisdir("/run")) {
		_d("System with new /run tmpfs ...");
		if (!fisdir("/run/lock"))
			makedir("/run/lock", 1777);
		ln("/run/lock", "/var/lock");
		ln("/dev/shm", "/run/shm");

		/* compat only, should really be set up by OS/dist */
		ln("/run", "/var/run");
	} else {
		makedir("/var/lock", 1777);
		makedir("/var/run", 0755);
	}
	makedir("/var/log",        0755);
	makedir("/var/mail",       0755);
	makedir("/var/opt",        0755);
	makedir("/var/spool",      0755);
	makedir("/var/spool/cron", 0755);
	makedir("/var/tmp",        0755);
	makedir("/var/empty",      0755);

	/*
	 * UTMP actually needs multiple db files
	 */
	if (has_utmp()) {
		int gid;

		/*
		 * If /etc/group or "utmp" group is missing, default to
		 * "root", or "wheel", group.
		 */
		_d("Setting up necessary UTMP files ...");
		gid = getgroup("utmp");
		if (gid < 0)
			gid = 0;

		create("/var/run/utmp",    0644, 0, gid); /* Currently logged in */
		create("/var/log/wtmp",    0644, 0, gid); /* Login history       */
		create("/var/log/btmp",    0600, 0, gid); /* Failed logins       */
		create("/var/log/lastlog", 0644, 0, gid);
	}

	/* Set BOOT_TIME UTMP entry */
	utmp_set_boot();

#ifdef TOUCH_ETC_NETWORK_RUN_IFSTATE
	touch("/etc/network/run/ifstate");
#else
	erase("/etc/network/run/ifstate");
#endif

	_d("Setting up misc files ...");
	makedir("/var/run/network",0755); /* Needed by Debian/Ubuntu ifupdown */
	makedir("/var/run/lldpd",  0755); /* Needed by lldpd */
	makedir("/var/run/pluto",  0755); /* Needed by Openswan */
	mksubsys("/var/run/quagga", 0755, "quagga", "quagga");
	mksubsys("/var/log/quagga", 0755, "quagga", "quagga");
	mksubsys("/var/run/frr",    0755, "frr", "frr");
	mksubsys("/var/log/frr",    0755, "frr", "frr");
	makedir("/var/run/sshd",  01755); /* OpenSSH  */

	if (!fexist("/etc/mtab"))
		ln("../proc/self/mounts", "/etc/mtab");

	/* Void Linux has a uuidd that runs as uuid:uuid and needs /run/uuid */
	mksubsys("/var/run/uuidd", 0755, "uuidd", "uuidd");

	/* Debian has /run/sudo, ensure correct perms and SELinux label */
	mksubsys("/var/run/sudo", 0711, "root", "root");
	mksubsys("/var/run/sudo/ts", 0700, "root", "root");
	if (whichp("restorecon"))
		run("restorecon /var/run/sudo /var/run/sudo/ts");

	umask(prev);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_MOUNT_POST] = { .cb = clean },
	.hook[HOOK_BASEFS_UP]  = { .cb = setup },
	.depends = { "pidfile" },
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
