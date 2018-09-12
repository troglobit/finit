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

#include <lite/lite.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "service.h"
#include "conf.h"

#define MODULES_LOAD_PATH "/etc/modules-load.d"
#define SERVICE_LINE \
	":%d name:modprobe.%s [2345] /sbin/modprobe %s %s -- Kernel module: %s"

static void load(void *arg)
{
	FILE *fp;
	DIR *dirp;
	struct dirent *direntp;
	char line[256];
	int index = 1;

	_d("Scanning " MODULES_LOAD_PATH " for config files ...");

	dirp = opendir(MODULES_LOAD_PATH);
	if (!dirp) return;

	while ((direntp = readdir(dirp)))
	{
		char module_path[PATH_MAX];
		strlcpy(module_path, MODULES_LOAD_PATH "/", sizeof(module_path));
		strlcat(module_path, direntp->d_name, sizeof(module_path));

		fp = fopen(module_path, "r");
		if (!fp) continue;

		while (fgets(line, sizeof(line), fp)) {
			char cmd[CMD_SIZE], *mod, *args;

			mod = strip_line(line);
			if (!*mod) continue;

			mod = chomp(mod);
			if (!mod || !*mod) continue;

			mod = strtok_r(mod, " ", &args);

			snprintf(cmd, sizeof(cmd), SERVICE_LINE,
					 index, mod, mod, args, mod);
			service_register(SVC_TYPE_TASK, cmd, global_rlimit, NULL);

			index++;
		}

		fclose(fp);
	}

	closedir(dirp);
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
