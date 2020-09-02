/* Simple pidfile event monitor for the Finit condition engine
 *
 * Copyright (c) 2015-2016  Tobias Waldekranz <tobias@waldekranz.com>
 * Copyright (c) 2016-2020 Joachim Wiberg <troglobit@gmail.com>
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

#include <glob.h>
#include <limits.h>
#include <paths.h>
#include <lite/queue.h>
#include <sys/inotify.h>

#include "finit.h"
#include "cond.h"
#include "helpers.h"
#include "pid.h"
#include "plugin.h"
#include "service.h"

struct wd_entry {
	TAILQ_ENTRY(wd_entry) link;
	char *path;
	int fd;
};

struct context {
	int fd;
	TAILQ_HEAD(, wd_entry) wd_list;
};

static struct context pidfile_ctx;

static int watcher_add(struct context *ctx, char *path)
{
	struct wd_entry *wd;
	uint32_t mask = IN_ONLYDIR | IN_CREATE | IN_ATTRIB | IN_DELETE | IN_MODIFY | IN_MOVED_TO;
	char *ptr;
	int fd;

	_d("pidfile: Adding new watcher for path %s", path);
	ptr = strstr(path, "run/");
	if (ptr) {
		char *slash;

		ptr += 4;
		slash = strchr(ptr, '/');
		if (slash) {
			ptr = slash++;
			slash = strchr(ptr, '/');
			if (slash) {
				_d("pidfile: Path too deep, skipping.");
				return -1;
			}
		}
	}

	fd = inotify_add_watch(ctx->fd, path, mask);
	if (fd < 0) {
		_pe("inotify_add_watch()");
		return -1;
	}

	wd = malloc(sizeof(struct wd_entry));
	if (!wd) {
		_pe("Failed allocating new `struct wd_entry`");
		inotify_rm_watch(ctx->fd, fd);
		return -1;
	}

	wd->path = path;
	wd->fd = fd;
	TAILQ_INSERT_HEAD(&ctx->wd_list, wd, link);

	return 0;
}

static int watcher_del(struct context *ctx, struct wd_entry *wde)
{
	_d("pidfile: Removing watcher for removed path %s", wde->path);
	TAILQ_REMOVE(&ctx->wd_list, wde, link);
	inotify_rm_watch(ctx->fd, wde->fd);
	free(wde->path);
	free(wde);

	return 0;
}

static void update_conds(char *dir, char *name, uint32_t mask)
{
	char cond[MAX_COND_LEN];
	char fn[PATH_MAX];
	svc_t *svc;

	_d("Got dir: %s, name: %s, mask: %08x", dir, name, mask);
	snprintf(fn, sizeof(fn), "%s/%s", dir, name);

	svc = svc_find_by_pidfile(fn);
	if (!svc) {
		_d("pidfile: No matching svc for %s", fn);
		return;
	}

	_d("pidfile: Found svc %s for %s with pid %d", svc->name, fn, svc->pid);

	mkcond(svc, cond, sizeof(cond));
	if (mask & (IN_CREATE | IN_ATTRIB | IN_MODIFY | IN_MOVED_TO)) {
		svc_started(svc);
		if (svc_is_forking(svc)) {
			pid_t pid;

			pid = pid_file_read(pid_file(svc));
			_d("Forking service %s changed PID from %d to %d",
			   svc->cmd, svc->pid, pid);
			svc->pid = pid;
		}

		cond_set(cond);
	} else if (mask & IN_DELETE)
		cond_clear(cond);
}

/* synthesize events in case of new run dirs */
static void scan_for_pidfiles(struct context *ctx, char *dir, int len)
{
	glob_t gl;
	size_t i;
	char path[len + 6];
	int rc;

	snprintf(path, sizeof(path), "%s/*.pid", dir);
	rc = glob(path, GLOB_NOSORT, NULL, &gl);
	if (rc && rc != GLOB_NOMATCH)
		return;

	snprintf(path, sizeof(path), "%s/pid", dir);
	rc = glob(path, GLOB_NOSORT | GLOB_APPEND, NULL, &gl);
	if (rc && rc != GLOB_NOMATCH)
		return;

	for (i = 0; i < gl.gl_pathc; i++) {
		_d("scan found %s", gl.gl_pathv[i]);
		update_conds(dir, gl.gl_pathv[i], IN_CREATE);
	}
	globfree(&gl);
}

static void handle_dir(struct context *ctx, struct wd_entry *wde, char *name, int mask)
{
	char *path;
	int plen;
	int exist = 0;

	plen = snprintf(NULL, 0, "%s/%s", wde->path, name) + 1;
	path = malloc(plen);
	if (!path) {
		_pe("Failed allocating path buffer for watcher");
		return;
	}
	snprintf(path, plen, "%s/%s", wde->path, name);
	_d("pidfile: path is %s", path);

	TAILQ_FOREACH(wde, &ctx->wd_list, link) {
		if (!strcmp(wde->path, path)) {
			exist = 1;
			break;
		}
	}

	if (mask & IN_CREATE) {
		if (!exist) {
			int rc = watcher_add(ctx, path);

			scan_for_pidfiles(ctx, path, plen);
			if (!rc)
				return;
		}
	} else if (mask & IN_DELETE) {
		if (exist)
			watcher_del(&pidfile_ctx, wde);
	}

	free(path);
}

static void pidfile_callback(void *arg, int fd, int events)
{
	struct inotify_event *ev;
	ssize_t sz, off;
	size_t buflen = 8 *(sizeof(struct inotify_event) + NAME_MAX + 1) + 1;
	char *buf;

        buf= malloc(buflen);
	if (!buf) {
		_pe("Failed allocating buffer for inotify events");
		return;
	}

	_d("pidfile: Entering ... reading %zu bytes into ev_buf[]", buflen - 1);
	sz = read(fd, buf, buflen - 1);
	if (sz <= 0) {
		_pe("invalid inotify event");
		goto done;
	}
	buf[sz] = 0;
	_d("pidfile: Read %zd bytes, processing ...", sz);

	off = 0;
	for (off = 0; off < sz; off += sizeof(*ev) + ev->len) {
		struct wd_entry *wde;

		ev = (struct inotify_event *)&buf[off];

		_d("pidfile: path %s, event: 0x%08x", ev->name, ev->mask);
		if (!ev->mask)
			continue;

		/* Find base path for this event */
		TAILQ_FOREACH(wde, &pidfile_ctx.wd_list, link) {
			if (wde->fd == ev->wd)
				break;
		}

		if (ev->mask & IN_ISDIR) {
			handle_dir(&pidfile_ctx, wde, ev->name, ev->mask);
			continue;
		}

		if (ev->mask & IN_DELETE) {
			_d("pidfile %s/%s removed ...", wde->path, ev->name);
			continue;
		}

		if (ev->mask & (IN_CREATE | IN_ATTRIB | IN_MODIFY | IN_MOVED_TO))
			update_conds(wde->path, ev->name, ev->mask);
	}
done:
	free(buf);
}

/*
 * This function is called after `initctl reload` to reassert conditions
 * for services that have not been changed.
 *
 * We reassert the run/task/service's condition only if it is running,
 * but not if it's recently been changed or while it's starting up.
 */
static void pidfile_reconf(void *arg)
{
	static char cond[MAX_COND_LEN];
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (svc->state != SVC_RUNNING_STATE)
			continue;

		if (svc_is_changed(svc) || svc_is_starting(svc))
			continue;

		mkcond(svc, cond, sizeof(cond));
		if (cond_get(cond) == COND_ON)
			continue;

		cond_set_path(cond_path(cond), COND_ON);
	}

	/*
	 * This will call service_step(), which in turn will schedule itself
	 * as long as stepped services change state.  Services going from
	 * WAITING to RUNNING will reassert their conditions in that loop,
	 * which in turn may unlock other services, and so on.
	 */
	service_step_all(SVC_TYPE_SERVICE | SVC_TYPE_RUNTASK | SVC_TYPE_INETD);
}

static void pidfile_init(void *arg)
{
	char *path;

	/*
	 * The bootmisc plugin is responsible for setting up /var/run.
	 * and/or /run, with proper symlinks etc.  We depend on bootmisc
	 * so it's safe here to query realpath() and set up inotify.
	 */
	path = realpath(_PATH_VARRUN, NULL);
	if (!path) {
		_pe("Failed ");
		return;
	}

	TAILQ_INIT(&pidfile_ctx.wd_list);
	if (watcher_add(&pidfile_ctx, path))
		close(pidfile_ctx.fd);
	_d("pidfile monitor active");
}

static struct context pidfile_ctx;

/*
 * We require /var/run to be set up before calling pidfile_init(),
 * so the bootmisc plugin must run first.
 */
static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP]  = { .cb = pidfile_init   },
	.hook[HOOK_SVC_RECONF] = { .cb = pidfile_reconf },
	.io = {
		.cb    = pidfile_callback,
		.flags = PLUGIN_IO_READ,
	},
	.depends = { "bootmisc", "netlink" },
};

PLUGIN_INIT(plugin_init)
{
	pidfile_ctx.fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (pidfile_ctx.fd < 0) {
		_pe("inotify_init()");
		return;
	}

	plugin.io.fd = pidfile_ctx.fd;
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	struct wd_entry *wde, *tmp;

	TAILQ_FOREACH_SAFE(wde, &pidfile_ctx.wd_list, link, tmp) {
		inotify_rm_watch(pidfile_ctx.fd, wde->fd);
		free(wde->path);
		free(wde);
	}
	close(pidfile_ctx.fd);

	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
