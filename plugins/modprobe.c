/* Coldplug modules using modalias magic
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

/*
 * Cold plug magic taken from Buildroot
 * https://github.com/buildroot/buildroot/commit/b4fc5a180c81689a982d5c595844331684c14f51
 *
 * The plugin basically does this, only in C:
 *
 * 	system("find /sys/devices -name modalias -print0"
 *             "     | xargs -0 sort -u -z"
 *             "     | xargs -0 modprobe -abq");
 *
 * Note: BusyBox must *not* be built with CONFIG_MODPROBE_SMALL
 */

#include <fnmatch.h>
#include <ftw.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>		/* gettimeofday() */
#include <sys/types.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "config.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"

struct module {
	TAILQ_ENTRY(module) link;
	char *alias;
};

static TAILQ_HEAD(, module) modules  = TAILQ_HEAD_INITIALIZER(modules);

static int modprobe(char *alias)
{
	char *args[] = {
		"modprobe",
		"-abq",
		alias,
		NULL,
	};
	pid_t pid;

	pid = fork();
	switch (pid) {
	case -1:
		_pe("Failed forking modprobe child");
		return 1;
	case 0:
		execvp(args[0], args);
		break;
	default:
		if (!complete(args[0], pid))
			_d("Successful modprobe of %s", alias);
		break;
	}

	return 0;
}

static void alias_add(char *alias)
{
	struct module *m;

	m = malloc(sizeof(*m));
	if (!m)
		return;
	m->alias = strdup(alias);
	if (!m->alias) {
		free(m);
		return;
	}

	TAILQ_INSERT_TAIL(&modules, m, link);
}

static void alias_remove(struct module *m)
{
	TAILQ_REMOVE(&modules, m, link);
	free(m->alias);
	free(m);
}	

static int alias_exist(char *alias)
{
	struct module *m;

	TAILQ_FOREACH(m, &modules, link) {
		if (!strcmp(m->alias, alias))
			return 1;
	}

	return 0;
}

static int scan_alias(const char *path, const struct stat *st, int flag, struct FTW *unused)
{
	FILE *fp;
	char buf[256];

	if (fnmatch("*/modalias", path, 0))
		return 0;

	fp = fopen(path, "r");
	if (!fp)
		return 0;
	if (fgets(buf, sizeof(buf), fp)) {
		chomp(buf);
		if (!alias_exist(buf))
			alias_add(buf);
	}
	fclose(fp);

	return 0;
}

static void coldplug(void *arg)
{
	struct module *m, *tmp;
	int rc = 0;

	/* Skip for systems without modules, e.g. small embedded or containers */
	if (!fisdir("/lib/modules"))
		return;

	if (!fismnt("/sys")) {
		print(1, "Cannot modprobe system, /sys is not mounted");
		return;
	}

	print_desc("Cold plugging system", NULL);
	rc = nftw("/sys/devices", scan_alias, 200, FTW_DEPTH | FTW_PHYS);
	if (!rc) {
		TAILQ_FOREACH_SAFE(m, &modules, link, tmp) {
			rc += modprobe(m->alias);
			alias_remove(m);
		}
	}

	print_result(rc);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = { .cb  = coldplug },
	.depends = { "bootmisc", }
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
