/* Simple pidfile event monitor for the Finit condition engine
 *
 * Copyright (c) 2015-2016  Tobias Waldekranz <tobias@waldekranz.com>
 * Copyright (c) 2016-2022  Joachim Wiberg <troglobit@gmail.com>
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

#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <paths.h>

#include "finit.h"
#include "cond.h"
#include "helpers.h"
#include "pid.h"
#include "plugin.h"
#include "service.h"
#include "iwatch.h"

static struct iwatch iw_pidfile;


static int pidfile_add_path(struct iwatch *iw, char *path)
{
	char *ptr;

	ptr = strstr(path, "run/");
	if (ptr) {
		char *slash;

		ptr += 4;
		slash = strchr(ptr, '/');
		if (slash) {
			ptr = slash++;
			slash = strchr(ptr, '/');
			if (slash) {
				_d("Path too deep, skipping.");
				return -1;
			}
		}
	}

	return iwatch_add(iw, path, IN_ONLYDIR | IN_CLOSE_WRITE);
}

static void pidfile_update_conds(char *dir, char *name, uint32_t mask)
{
	char cond[MAX_COND_LEN];
	char fn[PATH_MAX];
	svc_t *svc;

	paste(fn, sizeof(fn), dir, name);
	if (fnmatch("*\\.pid", fn, 0) && fnmatch("*/pid", fn, 0))
		return;

	_d("path: %s, mask: %08x", fn, mask);

	svc = svc_find_by_pidfile(fn);
	if (!svc) {
		_d("No matching svc for %s", fn);
		return;
	}

	_d("Found svc %s for %s with pid %d", svc->name, fn, svc->pid);

	mkcond(svc, cond, sizeof(cond));
	if (mask & (IN_CLOSE_WRITE | IN_ATTRIB | IN_MODIFY | IN_MOVED_TO)) {
		svc_started(svc);
		if (!svc_has_pidfile(svc)) {
			_d("Setting %s PID file to %s", svc->name, fn);
			pid_file_set(svc, fn, 1);
		}

		if (svc_is_forking(svc)) {
			pid_t pid;

			pid = pid_file_read(pid_file(svc));
			if (pid == -1) {
				cond_clear(cond);
				return;
			}

			/* Service is not crashing, let's tell state machine! */
			service_forked(svc);

			if (pid != svc->pid) {
				_d("Forking service %s changed PID from %d to %d",
				   svc->cmd, svc->pid, pid);
				svc->pid = pid;

				/* Complement log in service.c for non-forking services */
				logit(LOG_CONSOLE | LOG_NOTICE, "Started %s[%d]", svc_ident(svc, NULL, 0), pid);
			}
		}

		cond_set(cond);
	} else if (mask & IN_DELETE)
		cond_clear(cond);
}

/* synthesize events in case of new run dirs */
static void pidfile_scandir(struct iwatch *iw, char *dir, int len)
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
		pidfile_update_conds(dir, gl.gl_pathv[i], IN_CREATE);
	}
	globfree(&gl);
}

/*
 * create/remove sub-directory in monitored directory
 */
static void pidfile_handle_dir(struct iwatch *iw, char *dir, char *name, int mask)
{
	char path[strlen(dir) + strlen(name) + 2];
	struct iwatch_path *iwp;

	paste(path, sizeof(path), dir, name);
	_d("path: %s", path);

	iwp = iwatch_find_by_path(iw, path);

	if (mask & IN_CREATE) {
		if (!iwp) {
			if (!pidfile_add_path(iw, path))
				pidfile_scandir(iw, path, sizeof(path));
		}
	} else if (mask & IN_DELETE) {
		if (iwp)
			iwatch_del(&iw_pidfile, iwp);
	}
}

static void pidfile_callback(void *arg, int fd, int events)
{
	static char ev_buf[8 *(sizeof(struct inotify_event) + NAME_MAX + 1) + 1];
	struct inotify_event *ev;
	ssize_t sz;
	size_t off;

	sz = read(fd, ev_buf, sizeof(ev_buf) - 1);
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
		iwp = iwatch_find_by_wd(&iw_pidfile, ev->wd);
		if (!iwp)
			continue;

		if (ev->mask & IN_ISDIR) {
			pidfile_handle_dir(&iw_pidfile, iwp->path, ev->name, ev->mask);
			continue;
		}

		if (ev->mask & (IN_CLOSE_WRITE | IN_DELETE | IN_ATTRIB | IN_MODIFY | IN_MOVED_TO))
			pidfile_update_conds(iwp->path, ev->name, ev->mask);
	}
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
	service_step_all(SVC_TYPE_SERVICE | SVC_TYPE_RUNTASK);
}

static void pidfile_init(void *arg)
{
	char piddir[MAX_ARG_LEN];
	char *path;

	if (mkpath(pid_runpath(_PATH_CONDPID, piddir, sizeof(piddir)), 0755) && errno != EEXIST) {
		_pe("Failed creating %s condition directory, %s", COND_PID, _PATH_CONDPID);
		return;
	}

	/*
	 * The bootmisc plugin is responsible for setting up /var/run.
	 * and/or /run, with proper symlinks etc.  We depend on bootmisc
	 * so it's safe here to query realpath() and set up inotify.
	 */
	path = realpath(_PATH_VARRUN, NULL);
	if (!path) {
		_d("Failed querying realpath(%s): %s", _PATH_VARRUN, strerror(errno));
		path = realpath("/run", NULL);
		if (!path) {
			_e("System does not have %s or /run, aborting.", _PATH_VARRUN);
			return;
		}
	}

	if (pidfile_add_path(&iw_pidfile, path))
		iwatch_exit(&iw_pidfile);

	free(path);
}

/*
 * When performing an `initctl reload` with one (unchanged) service
 * depending on, e.g. `net/iface/lo`, its condition will not be set
 * to ON by the pidfile plugin unless the netlink plugin hook runs
 * first.
 *
 * Example:
 *
 *     service <net/iface/lo> /sbin/dropbear ...
 *
 * Which provides the <pid/dropbear> condition, will not be set by
 * pidfile.so during `initctl reload` because dropbear is still
 * SIGSTP:ed waiting for <net/iface/lo>.
 */
static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP]  = { .cb = pidfile_init   },
	.hook[HOOK_SVC_RECONF] = { .cb = pidfile_reconf },
	.depends = { "netlink" },
};

PLUGIN_INIT(plugin_init)
{
	int fd;

	fd = iwatch_init(&iw_pidfile);
	if (fd < 0)
		return;

	plugin.io.fd = fd;
	plugin.io.cb = pidfile_callback;
	plugin.io.flags = PLUGIN_IO_READ;

	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
	iwatch_exit(&iw_pidfile);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
