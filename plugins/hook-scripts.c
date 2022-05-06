/* Trigger scripts to run from finit's hooks
 *
 * Copyright (c) 2022  Tobias Waldekranz <tobias@waldekranz.com>
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

#include "finit.h"
#include "helpers.h"
#include "plugin.h"
#include "config.h"

#define CHOOSE(x, y) y
static const char *hscript_paths[] = HOOK_TYPES;
#undef CHOOSE

static void hscript_run_parts(hook_point_t hook)
{
	char path[CMD_SIZE] = "";

	strlcat(path, PLUGIN_HOOK_SCRIPTS_PATH, sizeof(path));
	strlcat(path, hscript_paths[hook] + 4, sizeof(path));

	run_parts(path, NULL);
}

static void hscript_banner(void *arg)
{
	hscript_run_parts(HOOK_BANNER);
}

static void hscript_rootfs_up(void *arg)
{
	hscript_run_parts(HOOK_ROOTFS_UP);
}

static void hscript_mount_error(void *arg)
{
	hscript_run_parts(HOOK_MOUNT_ERROR);
}

static void hscript_mount_post(void *arg)
{
	hscript_run_parts(HOOK_MOUNT_POST);
}

static void hscript_basefs_up(void *arg)
{
	hscript_run_parts(HOOK_BASEFS_UP);
}

static void hscript_network_up(void *arg)
{
	hscript_run_parts(HOOK_NETWORK_UP);
}

static void hscript_svc_up(void *arg)
{
	hscript_run_parts(HOOK_SVC_UP);
}

static void hscript_system_up(void *arg)
{
	hscript_run_parts(HOOK_SYSTEM_UP);
}

static void hscript_shutdown(void *arg)
{
	hscript_run_parts(HOOK_SHUTDOWN);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BANNER]      = { .cb  = hscript_banner      },
	.hook[HOOK_ROOTFS_UP]   = { .cb  = hscript_rootfs_up   },
	.hook[HOOK_MOUNT_ERROR] = { .cb  = hscript_mount_error },
	.hook[HOOK_MOUNT_POST]  = { .cb  = hscript_mount_post  },
	.hook[HOOK_BASEFS_UP]   = { .cb  = hscript_basefs_up   },
	.hook[HOOK_NETWORK_UP]  = { .cb  = hscript_network_up  },
	.hook[HOOK_SVC_UP]      = { .cb  = hscript_svc_up      },
	.hook[HOOK_SYSTEM_UP]   = { .cb  = hscript_system_up   },
	.hook[HOOK_SHUTDOWN]    = { .cb  = hscript_shutdown    },
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
