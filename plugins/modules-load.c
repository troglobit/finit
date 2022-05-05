/* Scans /etc/modules-load.d for modules to load.
 *
 * Copyright (c) 2018  Robert Andersson <robert.m.andersson@se.atlascopco.com>
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

/*
 * This plugin scans files in /etc/modules-load.d/, for each line in the
 * file it assumes the name of a kernel module, with optional arguments.
 * Each module is loaded when entering runlevel 2, 3, 4, or 5, using the
 * modprobe tool.
 *
 * Loading is done by inserting a `task` stanza in Finit, ensuring all
 * modules are loaded in parallel, with other modules and programs.
 * Each `task` stanza is by default given the name:modprobe.module and
 * indexed with `:ID`, starting with 1.
 *
 * Indexing can be disabled per file in /etc/modules-load.d/, anywwhere
 * in a file, by putting the keyword `noindex` on a line of its own.  A
 * noindex command can appear anywhere in the file, and up to the point
 * it is read, tasks will be indexed with an incrementing `:ID`.  When
 * `noindex` is read indexing is disabled and all subseequent tasks will
 * not have any `:ID` at all.
 *
 * Please note, the :ID is there for your benefit, it ensures that tasks
 * in Finit are unique.  If you have two tasks with the same name and ID
 * (or no ID), the last one read replaces any preceeding one!
 */

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "service.h"
#include "conf.h"

#ifndef MODULES_LOAD_PATH
#define MODULES_LOAD_PATH "/etc/modules-load.d"
#endif
#define SERVICE_LINE \
	"cgroup.init name:modprobe.%s :%d [2345] /sbin/modprobe %s %s -- Kernel module: %s"
#define SERVICE_LINE_NOINDEX \
	"cgroup.init name:modprobe.%s [2345] /sbin/modprobe %s %s -- Kernel module: %s"

static void load(void *arg)
{
	struct dirent *d;
	int counter = 1;
	FILE *fp;
	DIR *dir;

	_d("Scanning " MODULES_LOAD_PATH " for config files ...");

	dir = opendir(MODULES_LOAD_PATH);
	if (!dir)
		return;

	while ((d = readdir(dir))) {
		char module_path[PATH_MAX];
		int index = counter;
		char line[256];

		if (d->d_name[strlen(d->d_name) - 1] == '~')
			continue; /* skip backup files */

		strlcpy(module_path, MODULES_LOAD_PATH "/", sizeof(module_path));
		strlcat(module_path, d->d_name, sizeof(module_path));

		fp = fopen(module_path, "r");
		if (!fp)
			continue;

		while (fgets(line, sizeof(line), fp)) {
			char cmd[CMD_SIZE], *mod, *args;

			chomp(line);

			mod = strip_line(line);
			if (!mod[0])
				continue;

			if (mod[0] == '#' || mod[0] == ';')
				continue;

			if (!strcmp(mod, "set noindex")) {
				index = 0;
				continue;
			}
			if (!strncmp(mod, "set index", 9)) {
				mod += 9;
				args = strchr(mod, '=');
				if (args) /* optional */
					mod = args + 1;
				index = atoi(mod);
				continue;
			}

			mod = strtok_r(mod, " ", &args);
			if (!mod)
				continue;

			if (!index)
				snprintf(cmd, sizeof(cmd), SERVICE_LINE_NOINDEX, mod, mod, args, mod);
			else
				snprintf(cmd, sizeof(cmd), SERVICE_LINE, mod, index++, mod, args, mod);
			service_register(SVC_TYPE_TASK, cmd, global_rlimit, NULL);
			counter++;
		}

		fclose(fp);
	}

	closedir(dir);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = {
		.cb  = load
	},
};

PLUGIN_INIT(plugin_init)
{
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
