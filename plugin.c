/* Plugin based services architecture for finit
 *
 * Copyright (c) 2012  Joachim Nilsson <troglobit@gmail.com>
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
#include <dlfcn.h>		/* dlopen() et al */
#include <dirent.h>		/* readdir() et al */
#include <poll.h>
#include <string.h>
#include <sys/queue.h>		/* BSD sys/queue.h API */

#include "finit.h"
#include "private.h"
#include "helpers.h"
#include "plugin.h"

#define is_io_plugin(p) ((p)->io.cb && (p)->io.fd >= 0)

static size_t        num_fds = 0;
static struct pollfd fds[MAX_NUM_FDS];
static LIST_HEAD(, plugin) plugins = LIST_HEAD_INITIALIZER();

int plugin_register(plugin_t *plugin)
{
	int i, inuse = 0;

	if (!plugin) {
		errno = EINVAL;
		return 1;
	}

	if (!plugin->name) {
		Dl_info info;

		if (!dladdr(plugin, &info) || !info.dli_fname)
			plugin->name = "unknown";
		else
			plugin->name = (char *)info.dli_fname;
	}

	if (is_io_plugin(plugin)) {
		if (num_fds + 1 >= MAX_NUM_FDS) {
			num_fds = MAX_NUM_SVC;
			errno = ENOMEM;
			return 1;
		}
		num_fds++;
		inuse++;
	}

	if (plugin->svc.cb) {
		svc_t *svc = svc_find(plugin->name);

		if (svc) {
			inuse++;
			svc->cb           = plugin->svc.cb;
			svc->dynamic      = plugin->svc.dynamic;
			svc->dynamic_stop = plugin->svc.dynamic_stop;
		}
	}

	for (i = 0; i < HOOK_MAX_NUM; i++) {
		if (plugin->hook[i].cb)
			inuse++;
	}

	if (!inuse) {
		_e("No service \"%s\" loaded, and no I/O or finit hooks, skipping plugin.", basename(plugin->name));
		return 1;
	}

	LIST_INSERT_HEAD(&plugins, plugin, link);

	return 0;
}

int plugin_unregister(plugin_t *plugin)
{
	LIST_REMOVE(plugin, link);

	if (plugin->svc.cb) {
		svc_t *svc = svc_find(plugin->name);

		if (svc) {
			svc->cb      = NULL;
			svc->dynamic = 0;
		}
	}

	/* XXX: Unfinished, add cleanup code here! */

	return 0;
}

/* Private daemon API *******************************************************/
void plugin_run_hooks(hook_point_t no)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->hook[no].cb) {
			_d("Calling %s hook n:o %d from runloop...", basename(p->name), no);
			p->hook[no].cb(p->hook[no].arg);
		}
	}
}

/* Generic libev I/O callback, looks up correct plugin and calls its callback */
static void generic_io_cb(int fd, int events)
{
	plugin_t *p;

	/* Find matching plugin, pick first matching fd */
	PLUGIN_ITERATOR(p) {
		if (is_io_plugin(p) && p->io.fd == fd) {
			_d("Calling I/O %s from runloop...", basename(p->name));
			p->io.cb(p->io.arg, fd, events);
			break;
		}
	}
}

void plugin_monitor(void)
{
	int ret;
	size_t i;

	while ((ret = poll(fds, num_fds, 500))) {
		if (-1 == ret) {
			if (EINTR == errno)
				continue;

			_e("Failed polling I/O plugin descriptors, error %d: %s",
			   errno, strerror(errno));
			break;
		}

		/* Traverse all I/O fds and run callbacks */
		for (i = 0; i < num_fds; i++) {
			if (fds[i].revents)
				generic_io_cb(fds[i].fd, fds[i].revents);
		}

		break;
	}
}

/* Setup any I/O callbacks for plugins that use them */
static void init_plugins(void)
{
	int i = 0;
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (is_io_plugin(p)) {
			_d("Initializing plugin %s for I/O", basename(p->name));
			fds[i].revents    = 0;
			fds[i].events     = p->io.flags;
			fds[num_fds++].fd = p->io.fd;
		}
	}
}

int plugin_load_all(char *path)
{
	DIR *dp = opendir(path);
	struct dirent *entry;

	if (!dp) {
		_e("Failed, cannot open plugin directory %s: %s", path, strerror(errno));
		return 1;
	}

	while ((entry = readdir(dp))) {
		char *ext = entry->d_name + strlen(entry->d_name) - 3;

		if (!strcmp(ext, ".so")) {
			void *handle;
			char plugin[1024];

			snprintf(plugin, sizeof(plugin), "%s/%s", path, entry->d_name);
			print_desc("   Loading plugin ", basename(plugin));
			handle = dlopen(plugin, RTLD_LAZY | RTLD_GLOBAL);
			if (!handle) {
				_d("Failed loading plugin %s: %s", plugin, dlerror());
				print_result(1);
				return 1;
			}
			print_result(0);
		}
	}

	closedir(dp);
	init_plugins();

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
