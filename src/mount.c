/* Mount and unmount helpers
 *
 * Copyright (c) 2016-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <errno.h>
#include <string.h>
#include <mntent.h>
#include <sys/mount.h>

#include "helpers.h"

/*
 * SysV init on Debian/Ubuntu skips these protected mount points
 *
 * It also skips anything below /proc, /sys and /run for good measure so
 * we do the same here.  What we want is a list of non-protected tmpfs
 * and regular filesystems that can be safely unmounted before we do
 * swapoff, and remount / as read-only, respectively.
 */
static int is_protected(char *dir)
{
	size_t i;
	char *protected_mnts[] = {
		"/sys", "/proc",
		"/.dev", "/dev", "/dev/pts", "/dev/shm", "dev/.static/dev", "/dev/vcs",
		"/run", "/var/run",
		"/", NULL
	};
	char *any[] = {
		"/proc/", "/sys/", "/run/", NULL
	};

	for (i = 0; protected_mnts[i]; i++) {
		if (!strcmp(dir, protected_mnts[i]))
			return 1;
	}

	for (i = 0; any[i]; i++) {
		if (!strncmp(dir, any[i], strlen(any[i])))
			return 1;
	}

	return 0;
}

static void iterator_end(FILE **fp)
{
	endmntent(*fp);
	*fp = NULL;
}

static struct mntent *iterator(char *fstab, FILE **fp)
{
	static struct mntent *mnt;

	if (!*fp && fstab) {
		*fp = setmntent(fstab, "r");
		if (!*fp)
			return NULL;
	}

	while ((mnt = getmntent(*fp))) {
		if (is_protected(mnt->mnt_dir))
			continue;

		return mnt;
	}

	iterator_end(fp);
	return NULL;
}

static int unmount(const char *target)
{
	int rc;

	rc = umount(target);
	if (rc)
		print(2, "Failed unmounting %s, error %d: %s", target, errno, strerror(errno));

	return rc;
}

void unmount_tmpfs(void)
{
	struct mntent *mnt;
	FILE *fp = NULL;

	while ((mnt = iterator("/proc/mounts", &fp))) {
		if (!strcmp("tmpfs", mnt->mnt_fsname) && !unmount(mnt->mnt_dir))
			iterator_end(&fp);  /* Restart iteration */
	}
}

void unmount_regular(void)
{
	struct mntent *mnt;
	FILE *fp = NULL;

	while ((mnt = iterator("/proc/mounts", &fp))) {
		if (!unmount(mnt->mnt_dir))
			iterator_end(&fp);  /* Restart iteration */
	}
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
