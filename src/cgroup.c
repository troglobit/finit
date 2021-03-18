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
#include "iwatch.h"
#include "log.h"
#include "util.h"

static char controllers[256];

static struct iwatch iw_cgroup;
static uev_t cgw;


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
	echo(path, 0, "%d", pid);

	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s/cgroup.events", group, name);

	return iwatch_add(&iw_cgroup, path, 0);
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

static void cgroup_handle_event(char *event, uint32_t mask)
{
	char path[strlen(event)];
	char buf[80];
	char *ptr;
	FILE *fp;

	_d("event: '%s', mask: %08x", event, mask);
	if (!(mask & IN_MODIFY))
		return;

	fp = fopen(event, "r");
	while (fgets(buf, sizeof(buf), fp)) {
		if (strncmp(buf, "populated", 9))
			continue;

		chomp(buf);
		if (atoi(&buf[10]))
			break;

		strlcpy(path, event, sizeof(path));
		ptr = strrchr(path, '/');
		if (ptr) {
			*ptr = 0;
			rmdir(path);
		}

		break;
	}
}

static void cgroup_events_cb(uev_t *w, void *arg, int events)
{
	static char ev_buf[8 *(sizeof(struct inotify_event) + NAME_MAX + 1) + 1];
	struct inotify_event *ev;
	ssize_t sz;
	size_t off;

	sz = read(w->fd, ev_buf, sizeof(ev_buf) - 1);
	if (sz <= 0) {
		_pe("invalid inotify event");
		return;
	}
	ev_buf[sz] = 0;

	for (off = 0; off < (size_t)sz; off += sizeof(*ev) + ev->len) {
		struct iwatch_path *iwp;

		if (off + sizeof(*ev) > (size_t)sz)
			break;

		ev = (struct inotify_event *)&ev_buf[off];
		if (off + sizeof(*ev) + ev->len > (size_t)sz)
			break;

		if (!ev->mask)
			continue;

		/* Find base path for this event */
		iwp = iwatch_find_by_wd(&iw_cgroup, ev->wd);
		if (!iwp)
			continue;

		cgroup_handle_event(iwp->path, ev->mask);
	}

#ifdef ENABLE_AUTO_RELOAD
	if (conf_any_change())
		service_reload_dynamic();
#endif
}

/*
 * Called by Finit at early boot to mount initial cgroups
 */
void cgroup_init(uev_ctx_t *ctx)
{
	int opts = MS_NODEV | MS_NOEXEC | MS_NOSUID;
	char buf[80];
	FILE *fp;
	int fd;

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

	/* prepare cgroup.events watcher */
	fd = iwatch_init(&iw_cgroup);
	if (uev_io_init(ctx, &cgw, cgroup_events_cb, NULL, fd, UEV_READ)) {
		_pe("Failed setting up cgroup.events watcher");
		close(fd);
	}
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
