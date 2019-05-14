/* Finit control group support functions
 *
 * Copyright (c) 2019  Joachim Nilsson <troglobit@gmail.com>
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

#include "log.h"
#include "util.h"

static struct {
	int  active;
	char cgroup[16];
	char path[32];
} cg_avail[200];

static int cg_init = 0;
static int cg_num  = 0;


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
		mkdir(rc, 0755);
		if (mount("cgroup", rc, "cgroup", opts, cgroup)) {
			_d("Failed mounting %s cgroup on %s", cgroup, rc);
			continue;
		}

		cg_avail[cg_num].active = 1;
		strlcpy(cg_avail[cg_num].cgroup, cgroup, sizeof(cg_avail[cg_num].cgroup));
		strlcpy(cg_avail[cg_num].path, rc, sizeof(cg_avail[cg_num].path));
		cg_num++;

		/* Create default group in all available cgroups */
		strlcat(rc, "/default", sizeof(rc));
		mkdir(rc, 0755);
	}

	/* Default cgroups for process monitoring */
	mkdir("/sys/fs/cgroup/finit", 0755);
	if (mount("none", "/sys/fs/cgroup/finit", "cgroup", opts, "none,name=finit")) {
		_pe("Failed mounting Finit cgroup hierarchy");
		goto fail;
	}

	/* Set up reaper and enable callbacks */
	echo("/sys/fs/cgroup/finit/release_agent", 0, FINIT_LIBPATH_ "/cgreaper.sh");
	echo("/sys/fs/cgroup/finit/notify_on_release", 0, "1");

	/* Default groups, PID 1, services, and user/login processes */
	mkdir("/sys/fs/cgroup/finit/init", 0755);
	mkdir("/sys/fs/cgroup/finit/system", 0755);
	mkdir("/sys/fs/cgroup/finit/user", 0755);

	/* Move ourselves to init */
	echo("/sys/fs/cgroup/finit/init/cgroup.procs", 0, "1");

	/* We have signal, main screen turn on! */
	cg_init = 1;
fail:
	fclose(fp);
}

int cgroup_add(char *name, void (*cb)(char *, void *), void *arg)
{
	int rc = 0;
	int i;

	for (i = 0; i < cg_num; i++) {
		char path[80];

		snprintf(path, sizeof(path), "%s/%s", cg_avail[i].path, name);
		if (mkdir(path, 0755) && EEXIST != errno) {
			rc++;
			continue;
		}

		/* XXX: Add to cache of added groups, for cgroup_find() */
		if (cb)
			cb(path, arg);
	}

	return 0;
}

/* XXX: Temporary hackish implementation */
int cgroup_find(char *name, char *group, size_t len)
{
	char path[80];

	if (cg_num <= 0)
		return 1;

	snprintf(path, sizeof(path), "%s/%s", cg_avail[0].path, name);
	if (access(path, F_OK))
		return 1;

	strlcpy(group, name, len);

	return 0;
}

/* int cgroup_cpuset_range(char *group, int min, int max) */
/* { */
/* } */

static int move_pid(char *group, char *name, int pid)
{
	char path[256];

	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", group, name);
	if (mkdir(path, 0755) && errno != EEXIST)
		return 1;

	strlcat(path, "/cgroup.procs", sizeof(path));

	return echo(path, 0, "%d", pid);
}

int cgroup_user(char *name)
{
	if (!cg_init)
		return 0;

	return move_pid("finit/user", name, getpid());
}

int cgroup_service(char *cmd, int pid)
{
	char *nm;

	if (!cg_init)
		return 0;

	if (pid <= 0) {
		errno = EINVAL;
		return 1;
	}

	nm = strrchr(cmd, '/');
	if (nm)
		nm++;
	else
		nm = cmd;

	return move_pid("finit/system", nm, pid);
}

int cgroup_assign(char *name, int pid)
{
	char path[120];
	int i, rc = 0;

	for (i = 0; i < cg_num; i++) {
		snprintf(path, sizeof(path), "%s/%s/cgroup.procs",
			 cg_avail[i].path, name);
		rc += echo(path, 0, "%d", pid);
	}

	return rc;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
