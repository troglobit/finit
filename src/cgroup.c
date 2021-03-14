/* Finit control group support functions
 *
 * Copyright (c) 2019-2021  Joachim Wiberg <troglobit@gmail.com>
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
#include <stdio.h>
#include <string.h>
#include <lite/lite.h>
#include <sys/mount.h>
#include <sys/sysinfo.h>

#include "finit.h"
#include "log.h"
#include "util.h"

static char controllers[256] = "none";

static void group_init(char *path, char *nproc, int share)
{
	char file[strlen(path) + 64];

	paste(file, sizeof(file), path, "cgroup.clone_children");
	echo(file, 0, "1");

	paste(file, sizeof(file), path, "cpuset.cpus");
	echo(file, 0, nproc);

	paste(file, sizeof(file), path, "cpuset.mems");
	echo(file, 0, "0");

	paste(file, sizeof(file), path, "cpu.shares");
	echo(file, 0, "%d", share);

	paste(file, sizeof(file), path, "memory.use_hierarchy");
	echo(file, 0, "1");
}

static void append_ctrl(char *ctrl)
{
	char *wanted[] = {
		"cpu",
		"cpuacct",
		"cpuset",
		"memory"
	};
	size_t i;

	for (i = 0; i < NELEMS(wanted); i++) {
		if (strcmp(wanted[i], ctrl))
			continue;

		if (strcmp(controllers, "none"))
			strlcat(controllers, ",", sizeof(controllers));
		else
			strlcpy(controllers, "", sizeof(controllers));

		strlcat(controllers, ctrl, sizeof(controllers));
	}
}

/*
 * Called by Finit at early boot to mount initial cgroups
 */
void cgroup_init(void)
{
	int opts = MS_NODEV | MS_NOEXEC | MS_NOSUID;
	char buf[80];
	FILE *fp;

	fp = fopen("/proc/cgroups", "r");
	if (!fp) {
		_d("No cgroup support");
		return;
	}

	if (mount("none", "/sys/fs/cgroup", "tmpfs", opts, NULL)) {
		_d("Failed mounting cgroups");
		goto fail;
	}

	/* Skip first line, header */
	(void)fgets(buf, sizeof(buf), fp);

	/* Create and mount traditional cgroups v1 hier */
	while (fgets(buf, sizeof(buf), fp)) {
		char *cgroup;
#if 0
		char rc[80];
#endif

		cgroup = strtok(buf, "\t ");
		if (!cgroup)
			continue;

#if 0
		snprintf(rc, sizeof(rc), "/sys/fs/cgroup/%s", cgroup);
		if (mkdir(rc, 0755) && EEXIST != errno)
			continue;

		if (mount("cgroup", rc, "cgroup", opts, cgroup))
			_d("Failed mounting %s cgroup on %s", cgroup, rc);
#else
		append_ctrl(cgroup);
#endif
	}

	/* Finit default cgroups for process monitoring */
	if (mkdir(FINIT_CGPATH, 0755) && EEXIST != errno)
		goto fail;

#if 0
	strlcpy(controllers, "none,name=finit", sizeof(controllers));
#else
	strlcat(controllers, ",name=finit", sizeof(controllers));
#endif
	if (mount("none", FINIT_CGPATH, "cgroup", opts, controllers)) {
		_pe("Failed mounting Finit cgroup hierarchy");
		goto fail;
	}

	/* Set up reaper and enable callbacks */
	echo(FINIT_CGPATH "/release_agent", 0, FINIT_LIBPATH_ "/cgreaper.sh");
	echo(FINIT_CGPATH "/notify_on_release", 0, "1");

	/* Default groups, PID 1, services, and user/login processes */
	if (mkdir(FINIT_CGPATH "/init", 0755) && EEXIST != errno)
		goto fail;
	if (mkdir(FINIT_CGPATH "/system", 0755) && EEXIST != errno)
		goto fail;
	if (mkdir(FINIT_CGPATH "/user", 0755) && EEXIST != errno)
		goto fail;

	/*
	 * Set up basic limits for our groups.  Finit optimized defaults
	 * for embedded and server systems include limiting the groups
	 * init and user to a single CPU with 10% share, the system
	 * group gets the rest.  Override this from finit.conf
	 */
#if 0
#else
	snprintf(buf, sizeof(buf), "0-%d", get_nprocs_conf() - 1);

	group_init(FINIT_CGPATH "/init",   "0", 50);
	group_init(FINIT_CGPATH "/user",   "0", 75);
	group_init(FINIT_CGPATH "/system", buf, 900);
#endif

	/* Move ourselves to init */
	echo(FINIT_CGPATH "/init/cgroup.procs", 0, "1");

fail:
	fclose(fp);
}

static int move_pid(char *group, char *name, int pid)
{
	char path[256];

	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", group, name);
	if (mkdir(path, 0755) && errno != EEXIST)
		return 1;

	strlcat(path, "/cgroup.procs", sizeof(path));

	return echo(path, 0, "%d", pid);
}

int cgroup_user(char *name, int pid)
{
	return move_pid("finit/user", name, pid);
}

int cgroup_service(char *name, int pid)
{
	if (pid <= 0) {
		errno = EINVAL;
		return 1;
	}

	return move_pid("finit/system", name, pid);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
