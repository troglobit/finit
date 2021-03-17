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
#include <sys/sysinfo.h>		/* get_nprocs_conf() */

#include "finit.h"
#include "log.h"
#include "util.h"

static char controllers[256];

static void group_init(char *path, int leaf, int weight)
{
	char file[strlen(path) + 64];

	if (mkdir(path, 0755) && EEXIST != errno) {
		_pe("Failed creating cgroup %s", path);
		return;
	}

	/* enable detected controllers on domain groups */
	if (!leaf) {
		paste(file, sizeof(file), path, "cgroup.subtree_control");
		echo(file, 0, controllers);
	}

	/* set initial group weight */
	if (weight) {
		paste(file, sizeof(file), path, "cpu.weight");
		echo(file, 0, "%d", weight);
	}
}

static int cgroup_leaf_init(char *group, char *name, int pid)
{
	char path[256];

	if (pid <= 1) {
		errno = EINVAL;
		return 1;
	}

	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", group, name);
	group_init(path, 1, 0);

	strlcat(path, "/cgroup.procs", sizeof(path));

	return echo(path, 0, "%d", pid);
}

int cgroup_user(char *name, int pid)
{
	return cgroup_leaf_init("user", name, pid);
}

int cgroup_service(char *name, int pid)
{
	return cgroup_leaf_init("system", name, pid);
}

static void append_ctrl(char *ctrl)
{
	if (controllers[0])
		strlcat(controllers, " ", sizeof(controllers));

	strlcat(controllers, "+", sizeof(controllers));
	strlcat(controllers, ctrl, sizeof(controllers));
}

/*
 * Called by Finit at early boot to mount initial cgroups
 */
void cgroup_init(void)
{
	int opts = MS_NODEV | MS_NOEXEC | MS_NOSUID;
	char buf[80];
	FILE *fp;

	if (mount("none", FINIT_CGPATH, "cgroup2", opts, NULL)) {
		_pe("Failed mounting cgroup v2");
		return;
	}

	/* Find available controllers */
	fp = fopen(FINIT_CGPATH "/cgroup.controllers", "r");
	if (!fp) {
		_pe("Failed opening %s", FINIT_CGPATH "/cgroup.controllers");
		return;
	}

	if (fgets(buf, sizeof(buf), fp)) {
		char *cgroup;

		cgroup = strtok(chomp(buf), "\t ");
		while (cgroup) {
			append_ctrl(cgroup);
			cgroup = strtok(NULL, "\t ");
		}
	}

	fclose(fp);

	/* Enable all controllers */
	echo(FINIT_CGPATH "/cgroup.subtree_control", 0, controllers);

	/* Default groups, PID 1, services, and user/login processes */
	group_init(FINIT_CGPATH "/init",   1,  100);
	group_init(FINIT_CGPATH "/user",   0,  100);
	group_init(FINIT_CGPATH "/system", 0, 9800);

	/* Move ourselves to init */
	echo(FINIT_CGPATH "/init/cgroup.procs", 0, "1");
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
