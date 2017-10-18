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

#include <ftw.h>
#include <mntent.h>
#include <string.h>
#include <lite/lite.h>

#include "config.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"
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

static int do_clean(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftw)
{
	if (ftw->level == 0)
		return 1;

	_d("Removing %s ...", fpath);
	(void)remove(fpath);

	return 0;
}

/* We can safely skip tmpfs, nothing to clean from previous boot there */
static void bootclean(void)
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

		nftw(dir[i], do_clean, 20, FTW_DEPTH);
	}
}

/*
 * Setup standard FHS 2.3 structure in /var, and write runlevel to UTMP
 */
static void setup(void *arg)
{
	int uid, gid;

	/* Cleanup stale files, if any still linger on. */
	bootclean();

	_d("Setting up FHS structure in /var ...");
	umask(0);
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
	makedir("/var/log",        0755);
	makedir("/var/mail",       0755);
	makedir("/var/opt",        0755);
	makedir("/var/spool",      0755);
	makedir("/var/spool/cron", 0755);
	makedir("/var/tmp",        0755);
	makedir("/var/empty",      0755);

	/*
	 * If /etc/group or "utmp" group is missing,
	 * default to "root"/"wheel" group
	 */
	_d("Setting up necessary UTMP files ...");
	gid = getgroup("utmp");
	if (gid < 0)
		gid = 0;

	/*
	 * UTMP actually needs multiple db files
	 */
	create("/var/run/utmp",    0644, 0, gid); /* Currently logged in */
	create("/var/log/wtmp",    0644, 0, gid); /* Login history       */
	create("/var/log/btmp",    0600, 0, gid); /* Failed logins       */
	create("/var/log/lastlog", 0644, 0, gid);

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
	makedir("/var/run/quagga", 0755); /* Needed by Quagga */
	makedir("/var/log/quagga", 0755); /* Needed by Quagga */
	makedir("/var/run/sshd",  01755); /* OpenSSH  */

	gid = getgroup("tty");
	if (gid < 0)
		gid = 0;
	makefifo("/dev/xconsole",  0640); /* sysklogd */
	chown("/dev/xconsole", 0, gid);

	/* Void Linux has a uuidd that runs as uuid:uuid and needs /run/uuid */
	uid = getuser("uuidd", NULL);
	if (uid >= 0) {
		gid = getgroup("uuidd");
		if (gid < 0)
			gid = 0;
		makedir("/var/run/uuidd", 0755);
		chown("/var/run/uuidd", uid, gid);
	}

	umask(022);
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
