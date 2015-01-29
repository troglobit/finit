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

#include "finit.h"
#include "private.h"
#include "helpers.h"
#include "plugin.h"
#include "queue.h"		/* BSD sys/queue.h API */
#include "lite.h"

#define is_io_plugin(p) ((p)->io.cb && (p)->io.fd >= 0)

static char                *plugpath = NULL; /* Set by first load. */
static TAILQ_HEAD(, plugin) plugins  = TAILQ_HEAD_INITIALIZER(plugins);

static void check_plugin_depends(plugin_t *plugin);

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

	/* Already registered? */
	if (plugin_find(plugin->name)) {
		_d("... %s already loaded.", plugin->name);
		return 0;
	}

	check_plugin_depends(plugin);

	if (is_io_plugin(plugin))
		inuse++;

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
		_d("No service \"%s\" loaded, and no I/O or finit hooks, skipping plugin.", basename(plugin->name));
		return 1;
	}

	TAILQ_INSERT_TAIL(&plugins, plugin, link);

	return 0;
}

int plugin_unregister(plugin_t *plugin)
{
	TAILQ_REMOVE(&plugins, plugin, link);

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


//		_d("Comparing %s against plugin %s", str, p->name);
#define SEARCH_PLUGIN(str)						\
	PLUGIN_ITERATOR(p, tmp) {					\
		if (!strcmp(p->name, str))				\
			return p;					\
	}

/**
 * plugin_find - Find a plugin by name
 * @name: With or without path, or .so extension
 *
 * This function uses an opporunistic search for a suitable plugin and
 * returns the first match.  Albeit with at least some measure of
 * heuristics.
 *
 * First it checks for an exact match.  If no match is found and @name
 * starts with a slash the search ends.  Otherwise a new search with the
 * plugin path prepended to @name is made.  Also, if @name does not end
 * with .so it too is added to @name before searching.
 *
 * Returns:
 * On success the pointer to the matching &plugin_t is returned,
 * otherwise %NULL is returned.
 */
plugin_t *plugin_find(char *name)
{
	plugin_t *p, *tmp;

	if (!name) {
		errno = EINVAL;
		return NULL;
	}

	SEARCH_PLUGIN(name);

	if (plugpath && name[0] != '/') {
		int noext;
		char path[CMD_SIZE];

		noext = strcmp(name + strlen(name) - 3, ".so");
		snprintf (path, sizeof(path), "%s%s%s%s", plugpath,
			  fisslashdir(plugpath) ? "" : "/",
			  name, noext ? ".so" : "");

		SEARCH_PLUGIN(path);
	}

	errno = ENOENT;
	return NULL;
}

/* Private daemon API *******************************************************/
void plugin_run_hooks(hook_point_t no)
{
	plugin_t *p, *tmp;

	PLUGIN_ITERATOR(p, tmp) {
		if (p->hook[no].cb) {
			_d("Calling %s hook n:o %d from runloop...", basename(p->name), no);
			p->hook[no].cb(p->hook[no].arg);
		}
	}
}

/* Generic libev I/O callback, looks up correct plugin and calls its callback */
static void generic_io_cb(uev_ctx_t *UNUSED(ctx), uev_t *w, void *arg, int events)
{
	plugin_t *p = (plugin_t *)arg;

	if (is_io_plugin(p) && p->io.fd == w->fd) {
		_d("Calling I/O %s from runloop...", basename(p->name));
		p->io.cb(p->io.arg, w->fd, events);

		/* Update fd, may be changed by plugin callback, e.g., if FIFO */
		if (p->io.fd != w->fd)
			uev_io_set(w, p->io.fd, p->io.flags);
	}
}

/* Setup any I/O callbacks for plugins that use them */
static void init_plugins(uev_ctx_t *ctx)
{
	plugin_t *p, *tmp;

	PLUGIN_ITERATOR(p, tmp) {
		if (is_io_plugin(p)) {
			_d("Initializing plugin %s for I/O", basename(p->name));
			uev_io_init(ctx, &p->watcher, generic_io_cb, p, p->io.fd, p->io.flags);
		}
	}
}


/**
 * load_one - Load one plugin
 * @path: Path to finit plugins, usually %PLUGIN_PATH
 * @name: Name of plugin, optionally ending in ".so"
 *
 * Loads a plugin from @path/@name[.so].  Note, if ".so" is missing from
 * the plugin @name it is added before attempting to load.
 *
 * It is up to the plugin itself ot register itself as a "ctor" with the
 * %PLUGIN_INIT macro so that plugin_register() is called automatically.
 *
 * Returns:
 * POSIX OK(0) on success, non-zero otherwise.
 */
static int load_one(char *path, char *name)
{
	int noext;
	char plugin[CMD_SIZE];
	void *handle;

	if (!path || !fisdir(path) || !name) {
		errno = EINVAL;
		return 1;
	}

	/* Compose full path, with optional .so extension, to plugin */
	noext = strcmp(name + strlen(name) - 3, ".so");
	snprintf(plugin, sizeof(plugin), "%s/%s%s", path, name, noext ? ".so" : "");

	_d("Loading plugin %s ...", basename(plugin));
	/* Ignore leak, we do not support unloading of plugins atm.
	 * TODO: Add handle to list of loaded plugins, dlclose() on
	 *       plugin_unregister() */
	handle = dlopen(plugin, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle) {
		_e("Failed loading plugin %s: %s", plugin, dlerror());
		return 1;
	}

	return 0;
}

/**
 * check_plugin_depends - Check and load any plugins this one depends on.
 * @plugin: Plugin with possible depends to check
 *
 * Very simple dependency resolver, should actually load the plugin of
 * the correct .name, but currently loads a matching filename.
 *
 * Works, but only for now.  A better way might be to have a try_load()
 * that actually loads all plugins and checks their &plugin_t for the
 * correct .name.
 */
static void check_plugin_depends(plugin_t *plugin)
{
	int i;

	for (i = 0; i < PLUGIN_DEP_MAX && plugin->depends[i]; i++) {
//		_d("Plugin %s depends on %s ...", plugin->name, plugin->depends[i]);
		if (plugin_find(plugin->depends[i])) {
//			_d("OK plugin %s was already loaded.", plugin->depends[i]);
			continue;
		}

		load_one(plugpath, plugin->depends[i]);
	}
}

int plugin_load_all(uev_ctx_t *ctx, char *path)
{
	int fail = 0;
	DIR *dp = opendir(path);
	struct dirent *entry;

	if (!dp) {
		_e("Failed, cannot open plugin directory %s: %s", path, strerror(errno));
		return 1;
	}
	plugpath = path;

	while ((entry = readdir(dp))) {
		if (entry->d_name[0] == '.')
			continue; /* Skip . and .. directories */

//		print_desc("   Loading plugin ", basename(plugin));
		if (load_one(path, entry->d_name))
			fail++;
//		print_result(result);
	}

	closedir(dp);
	init_plugins(ctx);

	return fail;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
