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
#include <string.h>
#include <sys/queue.h>		/* BSD sys/queue.h API */

#include "finit.h"
#include "private.h"
#include "helpers.h"
#include "plugin.h"
#include "svc.h"

static LIST_HEAD(, plugin) plugins = LIST_HEAD_INITIALIZER();

int plugin_register(plugin_t *plugin)
{
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

	if (plugin->svc.cb) {
		svc_t *svc = svc_find_by_name(plugin->name);

		if (!svc) {
			_e("No service \"%s\" loaded, skipping plugin.", basename(plugin->name));
			return 1;
		}
		plugin->svc.id = svc_id(svc);
		svc->plugin    = &plugin->svc;
	}

	LIST_INSERT_HEAD(&plugins, plugin, link);

	return 0;
}

int plugin_unregister(plugin_t *plugin)
{
	LIST_REMOVE(plugin, link);

	return 0;
}

/* Private daemon API *******************************************************/
void run_hooks(hook_point_t no)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->hook[no].cb) {
			_d("Calling %s hook n:o %d from runloop...", basename(p->name), no);
			p->hook[no].cb(p->hook[no].arg);
		}
	}
}

void run_services(void)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->svc.cb) {
			_d("Calling svc %s from runloop...", basename(p->name));
			p->svc.cb(p->svc.arg, 0);
		}
	}
}

/* Generic libev I/O callback, looks up correct plugin and calls its callback */
static void generic_io_cb(struct ev_loop *loop, ev_io *w, int revents)
{
	plugin_t *p;

	ev_io_stop(loop, w);

	/* Find matching plugin, pick first matching fd */
	PLUGIN_ITERATOR(p) {
		if (p->io.cb && p->io.fd == w->fd) {
			p->io.cb(p->io.arg, w->fd, revents);
			break;
		}
	}

	ev_io_start(loop, w);
}

/* Setup any I/O callbacks for plugins that use them */
static void init_plugins(struct ev_loop *loop)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->io.cb && p->io.fd >= 0) {
			ev_io *w = calloc(1, sizeof(ev_io));

			_d("Initializing plugin %s for I/O", basename(p->name));
			if (!w) {
				_e("Failed setting up plugin %s: Out of memory", basename(p->name));
				continue;
			}
			ev_io_init(w, generic_io_cb, p->io.fd, p->io.flags);
			ev_io_start(loop, w);
		}
	}
}

int load_plugins(struct ev_loop *loop, char *path)
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
			char filename[1024];

			snprintf(filename, sizeof(filename), "%s/%s", path, entry->d_name);
			print_descr("   Loading plugin ", basename(filename));
			handle = dlopen(filename, RTLD_LAZY);
			if (!handle) {
				_d("Failed loading plugin %s: %s", filename, dlerror());
				print_result(1);
				return 1;
			}
			print_result(0);
		}
	}

	closedir(dp);
	init_plugins(loop);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
