/* Finit control group support functions
 *
 * Copyright (c) 2019-2025  Joachim Wiberg <troglobit@gmail.com>
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
#ifdef _LIBITE_LITE
# include <libite/lite.h>
# include <libite/queue.h>	/* BSD sys/queue.h API */
#else
# include <lite/lite.h>
# include <lite/queue.h>	/* BSD sys/queue.h API */
#endif
#include <sys/mount.h>
#include <sys/sysinfo.h>		/* get_nprocs_conf() */

#include "cgroup.h"
#include "finit.h"
#include "iwatch.h"
#include "conf.h"
#include "service.h"
#include "log.h"
#include "util.h"

struct cg {
	TAILQ_ENTRY(cg) link;

	char *name;		/* top-level group name */
	char *cfg;		/* kernel settings */

	int  active;		/* for mark & sweep */
	int  is_protected;	/* for init/, user/, & system/ */
};

static TAILQ_HEAD(, cg) cgroups = TAILQ_HEAD_INITIALIZER(cgroups);

static char controllers[256];

static struct iwatch iw_cgroup;
static uev_t cgw;
static int avail;


static void cgset(const char *path, char *ctrl, char *prop)
{
	char *val;

	dbg("path %s, ctrl %s, prop %s", path ?: "NIL", ctrl ?: "NIL", prop ?: "NIL");
	if (!path || !ctrl) {
		errx(1, "Missing path or controller, skipping!");
		return;
	}

	if (!prop) {
		prop = strchr(ctrl, '.');
		if (!prop) {
			errx(1, "Invalid cgroup ctrl syntax: %s", ctrl);
			return;
		}

		*prop++ = 0;
	}

	val = strchr(prop, ':');
	if (!val) {
		errx(1, "Missing cgroup ctrl value, prop %s", prop);
		return;
	}
	*val++ = 0;

	/* unquote value, if quoted */
	if (unquote(&val, NULL)) {
		errx(1, "Syntax error, unterminated quote in %s/%s.%s=%s", path, ctrl, prop, val);
		return;
	}

	/* disallow sneaky relative paths */
	if (strstr(ctrl, "..") || strstr(prop, "..")) {
		errx(1, "Possible security violation; '..' not allowed in cgroup config!");
		return;
	}

	dbg("%s/%s.%s <= %s", path, ctrl, prop, val);
	if (fnwrite(val, "%s/%s.%s", path, ctrl, prop))
		err(1, "Failed setting %s/%s.%s = %s", path, ctrl, prop, val);
}

/*
 * Settings for a cgroup are on the form: cpu.weight:1234,mem.max:4321,...
 * Finit supports the short-form 'mem.', replacing it with 'memory.' when
 * writing the setting to the file system.
 */
static void group_init(char *path, int leaf, const char *cfg)
{
	char *ptr, *s;

	dbg("path %s, leaf %d, cfg %s", path, leaf, cfg ?: "NIL");
	if (!fisdir(path)) {
		if (mkdir(path, 0755)) {
			err(1, "Failed creating cgroup %s", path);
			return;
		}

		/* enable detected controllers on domain groups */
		if (!leaf && fnwrite(controllers, "%s/cgroup.subtree_control", path))
			err(1, "Failed enabling %s for %s", controllers, path);
	}

	if (!cfg || !cfg[0])
		return;

	s = strdupa(cfg);
	if (!s) {
		err(1, "Failed activating cgroup cfg for %s", path);
		return;
	}

	dbg("%s <=> %s", path, s);
	ptr = strtok(s, ",");
	while (ptr) {
		dbg("ptr: %s", ptr);
		if (!strncmp("mem.", ptr, 4))
			cgset(path, "memory", &ptr[4]);
		else
			cgset(path, ptr, NULL);

		ptr = strtok(NULL, ",");
	}
}

static int cgroup_leaf_init(char *group, char *name, int pid, const char *cfg)
{
	char path[256];

	dbg("group %s, name %s, pid %d, cfg %s", group, name, pid, cfg ?: "NIL");
	if (pid < 0 || pid == 1) {
		errno = EINVAL;
		return 1;
	}

	/* create and initialize new group */
	snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", group, name);
	group_init(path, 1, cfg);

	/* move process to new group */
	if (fnwrite(str("%d", pid), "%s/cgroup.procs", path))
		err(1, "Failed moving pid %d to group %s", pid, path);

	strlcat(path, "/cgroup.events", sizeof(path));

	return iwatch_add(&iw_cgroup, path, 0);
}

int cgroup_user(char *name, int pid)
{
	if (!avail)
		return 0;

	return cgroup_leaf_init("user", name, pid, NULL);
}

int cgroup_service(char *name, int pid, struct cgroup *cg)
{
	char *group = "system";

	if (!avail)
		return 0;

	if (cg && cg->name[0]) {
		char path[256];

		if (!strcmp(cg->name, "root"))
			return fnwrite(str("%d", pid), FINIT_CGPATH "/cgroup.procs");

		if (!strcmp(cg->name, "init"))
			return fnwrite(str("%d", pid), FINIT_CGPATH "/init/cgroup.procs");

		snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", cg->name);
		if (fisdir(path))
			group = cg->name;
	}

	return cgroup_leaf_init(group, name, pid, cg ? cg->cfg : NULL);
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
	char path[strlen(event) + 1];
	char buf[80];
	char *ptr;
	FILE *fp;

	dbg("event: '%s', mask: %08x", event, mask);
	if (!(mask & IN_MODIFY))
		return;

	fp = fopen(event, "r");
	if (!fp) {
		dbg("Failed opening %s, skipping ...", event);
		return;
	}

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
			if (!cgroup_del(path)) {
				/*
				 * try with parent, top-level group, we
				 * may get events out-of-order *sigh*
				 */
				ptr = strrchr(path, '/');
				if (!ptr)
					break;
				*ptr = 0;
				cgroup_del(path);
			}
		}

		break;
	}

	fclose(fp);
}

static void cgroup_events_cb(uev_t *w, void *arg, int events)
{
	static char ev_buf[8 *(sizeof(struct inotify_event) + NAME_MAX + 1) + 1];
	struct inotify_event *ev;
	ssize_t sz;
	size_t off;

	(void)arg;
	if (UEV_ERROR == events) {
		dbg("%s(): inotify socket %d invalid.", __func__, w->fd);
		return;
	}

	sz = read(w->fd, ev_buf, sizeof(ev_buf) - 1);
	if (sz <= 0) {
		err(1, "invalid inotify event");
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
		if (!iwp || !iwp->path)
			continue;

		cgroup_handle_event(iwp->path, ev->mask);
	}

#ifdef AUTO_RELOAD
	if (conf_any_change())
		sm_reload();
#endif
}

static struct cg *cgroup_find(char *name)
{
	struct cg *cg;

	TAILQ_FOREACH(cg, &cgroups, link) {
		if (strcmp(cg->name, name))
			continue;

		return cg;
	}

	return NULL;
}

/*
 * Marks all unprotected cgroups for deletion (during reload)
 */
void cgroup_mark_all(void)
{
	struct cg *cg;

	if (!avail)
		return;

	TAILQ_FOREACH(cg, &cgroups, link) {
		if (cg->is_protected)
			continue;

		cg->active = 0;
	}
}

/*
 * Remove (try to) all unused cgroups
 */
void cgroup_cleanup(void)
{
	struct cg *cg, *tmp;
	char path[256];

	if (!avail)
		return;

	TAILQ_FOREACH_SAFE(cg, &cgroups, link, tmp) {
		if (cg->active)
			continue;

		snprintf(path, sizeof(path), FINIT_CGPATH "/%s", cg->name);
		cgroup_del(path);
	}
}

/*
 * Add, or update, settings for top-level cgroup
 */
int cgroup_add(char *name, char *cfg, int is_protected)
{
	struct cg *cg;

	if (!avail)
		return 0;

	if (!name)
		return -1;
	if (!cfg)
		cfg = "";

	cg = cgroup_find(name);
	if (!cg) {
		cg = malloc(sizeof(struct cg));
		if (!cg) {
			err(1, "Failed allocating 'struct cg' for %s", name);
			return -1;
		}
		cg->name = strdup(name);
		if (!cg->name) {
			err(1, "Failed setting cgroup name %s", name);
			free(cg);
			return -1;
		}
		TAILQ_INSERT_TAIL(&cgroups, cg, link);
	} else
		free(cg->cfg);

	cg->cfg = strdup(cfg);
	if (!cg->cfg) {
		err(1, "Failed add/update of cgroup %s", name);
		TAILQ_REMOVE(&cgroups, cg, link);
		free(cg->name);
		free(cg);
		return -1;
	}
	cg->is_protected = is_protected;
	cg->active = 1;

	return 0;
}

/*
 * Remove inactive top-level cgroup
 */
int cgroup_del(char *dir)
{
	struct cg *cg;
	char path[256];

	if (!avail)
		return 0;

	TAILQ_FOREACH(cg, &cgroups, link) {
		snprintf(path, sizeof(path), FINIT_CGPATH "/%s", cg->name);
		if (strcmp(path, dir))
			continue;

		if (cg->active)
			return -1;

		break;
	}

	if (rmdir(dir) && errno != ENOENT) {
		dbg("Failed removing %s: %s", dir, strerror(errno));
		return -1;
	}

	if (cg) {
		TAILQ_REMOVE(&cgroups, cg, link);
		free(cg->name);
		free(cg->cfg);
		free(cg);
	}

	return 0;
}

/* the top-level init cgroup is a leaf, that's ensured in cgroup_init() */
void cgroup_config(void)
{
	struct cg *cg;

	if (!avail)
		return;

	TAILQ_FOREACH(cg, &cgroups, link) {
		char path[256];
		int leaf = 0;

		if (!cg->active)
			continue;
		if (!strcmp(cg->name, "init"))
			leaf = 1;	/* reserved */

		snprintf(path, sizeof(path), "%s/%s", FINIT_CGPATH, cg->name);
		group_init(path, leaf, cg->cfg);

		strlcat(path, "/cgroup.events", sizeof(path));
		iwatch_add(&iw_cgroup, path, 0);
	}
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

#ifndef CGROUP2_ENABLED
	avail = 0;
	return;
#endif

	if (mount("none", FINIT_CGPATH, "cgroup2", opts, NULL)) {
		if (errno == ENOENT)
			logit(LOG_INFO, "Kernel does not support cgroups v2, disabling.");
		else if (errno == EPERM) /* Probably inside an unprivileged container */
			logit(LOG_INFO, "Not allowed to mount cgroups v2, disabling.");
		else
			err(1, "Failed mounting cgroup v2");
		avail = 0;
		return;
	}
	avail = 1;

	/* Find available controllers */
	fp = fopen(FINIT_CGPATH "/cgroup.controllers", "r");
	if (!fp) {
		err(1, "Failed opening %s", FINIT_CGPATH "/cgroup.controllers");
	abort:
		umount(FINIT_CGPATH);
		avail = 0;
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

	/* Check for cpu controller, abort if missing */
	if (!strstr(controllers, "+cpu")) {
		logit(LOG_NOTICE, "Missing CPU controller, disabling cgroup support.");
		goto abort;
	}

	/* Enable all controllers */
	if (fnwrite(controllers, FINIT_CGPATH "/cgroup.subtree_control")) {
		err(1, "Failed enabling %s for %s", controllers, FINIT_CGPATH "/cgroup.subtree_control");
		goto abort;
	}

	/* prepare cgroup.events watcher */
	fd = iwatch_init(&iw_cgroup);
	if (fd == -1)
		goto abort;

	if (uev_io_init(ctx, &cgw, cgroup_events_cb, NULL, fd, UEV_READ)) {
		err(1, "Failed setting up cgroup.events watcher");
		iwatch_exit(&iw_cgroup);
		close(fd);
		goto abort;
	}

	/* Default (protected) groups, PID 1, services, and user/login processes */
	cgroup_add("init",   "cpu.weight:100",  1);
	cgroup_add("system", "cpu.weight:9800", 1);
	cgroup_add("user",   "cpu.weight:100",  1);
	cgroup_config();

	/* Move ourselves to init (best effort, otherwise run in 'root' group */
	if (fnwrite("1", FINIT_CGPATH "/init/cgroup.procs")) {
		err(1, "Failed moving PID 1 to cgroup %s", FINIT_CGPATH "/init");
		uev_io_stop(&cgw);
		iwatch_exit(&iw_cgroup);
		close(fd);
		goto abort;
	}
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
