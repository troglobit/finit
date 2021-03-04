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

#include "finit.h"
#include "log.h"
#include "util.h"

static int cg_init = 0;

/*
 * Called by Finit at early boot to mount initial cgroups
 */
void cgroup_init(void)
{
	FILE *fp;
	char buf[80];
	int opts = MS_NODEV | MS_NOEXEC | MS_NOSUID;

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
	fgets(buf, sizeof(buf), fp);
	while (fgets(buf, sizeof(buf), fp)) {
		char *cgroup;
		char rc[80];

		cgroup = strtok(buf, "\t ");
		if (!cgroup)
			continue;

		snprintf(rc, sizeof(rc), "/sys/fs/cgroup/%s", cgroup);
		if (mkdir(rc, 0755) && EEXIST != errno)
			continue;
		if (mount("cgroup", rc, "cgroup", opts, cgroup))
			_d("Failed mounting %s cgroup on %s", cgroup, rc);
	}

	/* Default cgroups for process monitoring */
	if (mkdir(FINIT_CGPATH, 0755) && EEXIST != errno)
		goto fail;
	if (mount("none", FINIT_CGPATH, "cgroup", opts, "none,name=finit")) {
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

	/* Move ourselves to init */
	echo(FINIT_CGPATH "/init/cgroup.procs", 0, "1");

	/* We have signal, main screen turn on! */
	cg_init = 1;
fail:
	fclose(fp);
}

static int move_pid(char *group, char *name, char *id, int pid)
{
	char path[256];

	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", group, name);
	if (id && strlen(id)) {
		strlcat(path, ":", sizeof(path));
		strlcat(path, id, sizeof(path));
	}
	if (mkdir(path, 0755) && errno != EEXIST)
		return 1;

	strlcat(path, "/cgroup.procs", sizeof(path));

	return echo(path, 0, "%d", pid);
}

int cgroup_user(char *name)
{
	if (!cg_init)
		return 0;

	return move_pid("finit/user", name, "0", getpid());
}

int cgroup_service(char *nm, char *id, int pid)
{
	if (!cg_init)
		return 0;

	if (pid <= 0) {
		errno = EINVAL;
		return 1;
	}

	return move_pid("finit/system", nm, id, pid);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
