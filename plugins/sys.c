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
static int num_ac_online;
static int num_ac;

static void sys_cond(int set)
{
	char cond[MAX_COND_LEN] = COND_SYS;

	strlcat(cond, "pwr/ac", sizeof(cond));
	if (set)
		cond_set_oneshot(cond);
	else
		cond_clear(cond);
}

static int check_online(char *ac)
{
	char path[strlen(ac) + 1];
	char *ptr;
	int val;

	strlcpy(path, ac, sizeof(path));
	ptr = strrchr(path, '/');
	if (!ptr)
		return 0;
	ptr[1] = 0;
	strlcat(path, "online", sizeof(path));

	if (fngetint(path, &val))
		return 0;

	return val;
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

		if (check_online(iwp->path)) {
			if (!num_ac_online)
				sys_cond(1);

			if (num_ac_online < num_ac)
				num_ac_online++;
		} else {
			if (num_ac_online > 0)
				num_ac_online--;

			if (!num_ac_online)
				sys_cond(0);
		}
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

/*
 * Called when base filesystem is up, modules have been probed,
 * or insmodded from /etc/finit.conf, so by now we should have
 * /sys/class/power_supply/ available for probing.  If none is
 * found we assert /sys/pwr/ac condition anyway, this is what
 * systemd does (ConditionACPower) and also makes most sense.
 */
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

			snprintf(path, sizeof(path), "%s/%s/uevent", _PATH_SYSFS_PWR, nm);
			if (check_online(path))
				num_ac_online++;

			if (iwatch_add(&iw_sys, path, IN_CLOSE_NOWRITE))
				_pe("Failed setting up pwr monitor for %s", path);
		}
		free(d[i]);
	}
	free(d);

	/* if any power_supply is online, or none can be found */
	if (num_ac == 0 || num_ac_online > 0)
		sys_cond(1);
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
