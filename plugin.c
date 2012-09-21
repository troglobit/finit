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
#include <stdio.h>
#include <string.h>
#include <sys/queue.h>		/* BSD sys/queue.h API */

#include "plugin.h"

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
			plugin->name = info.dli_fname;
	}
	LIST_INSERT_HEAD(&plugins, plugin, link);

	return 0;
}

int plugin_unregister(plugin_t *plugin)
{
	LIST_REMOVE(plugin, link);

	return 0;
}

void run_hooks(hook_point_t no)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->hook[no].cb) {
			printf("Calling %s hook n:o %d from runloop...\n", p->name, no);
			p->hook[no].cb(p->hook[no].arg);
		}
	}
}

void run_services(void)
{
	plugin_t *p;

	PLUGIN_ITERATOR(p) {
		if (p->svc.cb) {
			printf("Calling svc %s from runloop...\n", p->name);
			p->svc.cb(p->svc.arg);
		}
	}
}

int load_plugins(char *path)
{
	DIR *dp = opendir(path);
	struct dirent *entry;

	if (!dp) {
		fprintf(stderr, "Failed, cannot open plugin directory %s: %s\n",
			path, strerror(errno));
		return 1;
	}

	while ((entry = readdir(dp))) {
		char *ext = entry->d_name + strlen(entry->d_name) - 3;

		if (!strcmp(ext, ".so")) {
			void *handle;
			char filename[1024];

			snprintf(filename, sizeof(filename), "%s/%s", path, entry->d_name);
			printf("Loading plugin %s ...\n", filename);
			handle = dlopen(filename, RTLD_LAZY);
			if (!handle) {
				fprintf(stderr,
					"Failed loading plugin %s: %s\n", filename, dlerror());
				return 1;
			}
		}
	}

	closedir(dp);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
