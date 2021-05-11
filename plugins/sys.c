/* System condtion event monitor for the Finit condition engine
 *
 * Copyright (c) 2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <dirent.h>
#include <fnmatch.h>
#include <glob.h>
#include <limits.h>
#include <paths.h>

#include "finit.h"
#include "cond.h"
#include "pid.h"
#include "plugin.h"
#include "iwatch.h"

#define _PATH_SYSFS_PWR  "/sys/class/power_supply"

static struct iwatch iw_sys;
static int num_ac;

static void check_online(char *path, int clear)
{
	char cond[MAX_COND_LEN] = COND_SYS;
	int val;

	if (fngetint(path, &val))
		return;

	strlcat(cond, "pwr/ac", sizeof(cond));
	if (val)
		cond_set_oneshot(cond);
	else if (clear)
		cond_clear(cond);
}

static void sys_callback(void *arg, int fd, int events)
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

		_d("name %s, event: 0x%08x", ev->name, ev->mask);
		if (!ev->mask)
			continue;

		/* Find base path for this event */
		iwp = iwatch_find_by_wd(&iw_sys, ev->wd);
		if (!iwp)
			continue;

		check_online(iwp->path, 1);
	}
}

static int is_ac(char *path, size_t len)
{
	char type[32] = { 0 };
	char *types[] = {
		"Mains",
		"USB",
		"BrickID",
		"Wireless",
		NULL
	};
	FILE *fp;
	int i;

	strlcat(path, "/type", len);
	fp = fopen(path, "r");
	if (!fp)
		return 0;

	if (!fgets(type, sizeof(type), fp)) {
		fclose(fp);
		return 0;
	}
	fclose(fp);

	for (i = 0; types[i]; i++) {
		if (!strncmp(type, types[i], strlen(types[i])))
			return 1;
	}

	return 0;
}

static void sys_init(void *arg)
{
	struct dirent **d = NULL;
	char path[384];
	int i, n;

	if (mkpath(pid_runpath(_PATH_CONDSYS, path, sizeof(path)), 0755) && errno != EEXIST) {
		_pe("Failed creating %s condition directory, %s", COND_SYS, _PATH_CONDSYS);
		return;
	}

	n = scandir(_PATH_SYSFS_PWR, &d, NULL, alphasort);
	if (n <= 0) {
		iwatch_exit(&iw_sys);
		return;
	}
		
	for (i = 0; i < n; i++) {
		char *nm = d[i]->d_name;

		snprintf(path, sizeof(path), "%s/%s", _PATH_SYSFS_PWR, nm);
		if (is_ac(path, sizeof(path))) {
			num_ac++;

			snprintf(path, sizeof(path), "%s/%s/online", _PATH_SYSFS_PWR, nm);
			check_online(path, 0);

			if (iwatch_add(&iw_sys, path, 0))
				_pe("Failed setting up pwr monitor for %s", path);
		}
		free(d[i]);
	}
	free(d);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP]  = { .cb = sys_init },
};

PLUGIN_INIT(plugin_init)
{
	int fd;

	if (!fisdir(_PATH_SYSFS_PWR)) {
		_w("System does not have %s, disabling AC power monitor.", _PATH_SYSFS_PWR);
		return;
	}

	fd = iwatch_init(&iw_sys);
	if (fd < 0)
		return;

	plugin.io.fd = fd;
	plugin.io.cb = sys_callback;
	plugin.io.flags = PLUGIN_IO_READ;

	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	iwatch_exit(&iw_sys);
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
