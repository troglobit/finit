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

#include "config.h"
#include "finit.h"
#include "private.h"
#include "helpers.h"
#include "plugin.h"
#include "queue.h"		/* BSD sys/queue.h API */
#include "lite/lite.h"

#define is_io_plugin(p) ((p)->io.cb && (p)->io.fd >= 0)

static char *plugpath = NULL; /* Set by first load. */
static TAILQ_HEAD(plugin_head, plugin) plugins  = TAILQ_HEAD_INITIALIZER(plugins);

#ifndef ENABLE_STATIC
static void check_plugin_depends(plugin_t *plugin);
#endif

int plugin_register(plugin_t *plugin)
{
	if (!plugin) {
		errno = EINVAL;
		return 1;
	}

	/* Setup default name if none is provided */
	if (!plugin->name) {
#ifndef ENABLE_STATIC
		Dl_info info;

		if (!dladdr(plugin, &info) || !info.dli_fname)
			plugin->name = "unknown";
		else
			plugin->name = (char *)info.dli_fname;
#else
			plugin->name = "unknown";
#endif
	}

	/* Already registered? */
	if (plugin_find(plugin->name)) {
		_d("... %s already loaded.", plugin->name);
		return 0;
	}

#ifndef ENABLE_STATIC
	/* Resolve plugin dependencies */
	check_plugin_depends(plugin);
#endif

	TAILQ_INSERT_TAIL(&plugins, plugin, link);

	return 0;
}

/* Not called, at the moment plugins cannot be unloaded. */
int plugin_unregister(plugin_t *plugin)
{
#ifndef ENABLE_STATIC
	TAILQ_REMOVE(&plugins, plugin, link);

	if (plugin->svc.cb) {
		svc_t *svc;

		/* Unregister plugin callback for all matching services */
		for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
			if (strcmp(svc->cmd, plugin->name))
				continue;

			svc->cb           = NULL;
			svc->dynamic      = 0;
			svc->dynamic_stop = 0;
		}
	}

	/* Unload plugin */
	if (dlclose(plugin->handle)) {
		_e("Failed unloading %s ...", plugin->name);
		return 1;
	}
#else
	_d("Finit built statically, cannot unload %s ...", plugin->name);
#endif

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

/* Some hooks are called with a fixed argument, like HOOK_SVC_LOST */
void plugin_run_hook(hook_point_t no, void *arg)
{
	plugin_t *p, *tmp;

	PLUGIN_ITERATOR(p, tmp) {
		if (p->hook[no].cb) {
			_d("Calling %s hook n:o %d (arg: %p) ...", basename(p->name), no, arg);
			p->hook[no].cb(arg ? arg : p->hook[no].arg);
		}
	}
}

/* Regular hooks are called with the registered plugin's argument */
void plugin_run_hooks(hook_point_t no)
{
	plugin_run_hook(no, NULL);
}

/* Generic libev I/O callback, looks up correct plugin and calls its callback */
static void generic_io_cb(uev_t *w, void *arg, int events)
{
	plugin_t *p = (plugin_t *)arg;

	if (is_io_plugin(p) && p->io.fd == w->fd) {
		/* Stop watcher, callback may close descriptor on us ... */
		uev_io_stop(w);

		_d("Calling I/O %s from runloop...", basename(p->name));
		p->io.cb(p->io.arg, w->fd, events);

		/* Update fd, may be changed by plugin callback, e.g., if FIFO */
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

#ifndef ENABLE_STATIC
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
	char sofile[CMD_SIZE];
	void *handle;
	plugin_t *plugin;

	if (!path || !fisdir(path) || !name) {
		errno = EINVAL;
		return 1;
	}

	/* Compose full path, with optional .so extension, to plugin */
	noext = strcmp(name + strlen(name) - 3, ".so");
	snprintf(sofile, sizeof(sofile), "%s/%s%s", path, name, noext ? ".so" : "");

	_d("Loading plugin %s ...", basename(sofile));
	/* Ignore leak, we do not support unloading of plugins atm.
	 * TODO: Add handle to list of loaded plugins, dlclose() on
	 *       plugin_unregister() */
	handle = dlopen(sofile, RTLD_LAZY | RTLD_GLOBAL);
	if (!handle) {
		_e("Failed loading plugin %s: %s", sofile, dlerror());
		return 1;
	}

	plugin = TAILQ_LAST(&plugins, plugin_head);
	if (!plugin) {
		_e("Plugin %s failed to register, unloading from memory.", sofile);
		dlclose(handle);
		return 1;
	}

	/* Remember handle from dlopen() for plugin_unregister() */
	plugin->handle = handle;

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
#endif	/* ENABLE_STATIC */

int plugin_load_all(uev_ctx_t *ctx, char *path)
{
	int fail = 0;
#ifndef ENABLE_STATIC
	DIR *dp = opendir(path);
	struct dirent *entry;

	if (!silent)
		print_desc("Loading plugins", NULL);
	if (!dp) {
		_e("Failed, cannot open plugin directory %s: %s", path, strerror(errno));
		fail = 1;
		goto exit;
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
#else
	_d("Finit built statically, not loading plugins from %s ...", path);
	if (!silent)
		print_desc("Initializing plugins", NULL);
#endif	/* ENABLE_STATIC */

	/* Always initialize plugins */
	init_plugins(ctx);

#ifndef ENABLE_STATIC
exit:
#endif
	if (!silent)
		print_result(fail);

	return fail;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
