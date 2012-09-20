/* Test of dlopen() for a plugin based services architecture for finit
 *
 * Copyright (c) 2012  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

int load_plugins(void)
{
	DIR *dp = opendir(PLUGIN_PATH);
	struct dirent *entry;

	if (!dp) {
		fprintf(stderr, "Failed, cannot open plugin directory %s: %s\n",
			PLUGIN_PATH, strerror(errno));
		return 1;
	}

	while ((entry = readdir(dp))) {
		char *ext = entry->d_name + strlen(entry->d_name) - 3;

		if (!strcmp(ext, ".so")) {
			void *handle;
			char filename[1024];

			snprintf(filename, sizeof(filename), "%s/%s", PLUGIN_PATH, entry->d_name);
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
