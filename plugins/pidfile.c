#include <limits.h>
#include <paths.h>

#include <sys/inotify.h>

#include "../finit.h"
#include "../cond.h"
#include "../helpers.h"
#include "../plugin.h"

struct context {
	int fd;
	int wd;
};

static void pidfile_callback(void *UNUSED(arg), int fd, int UNUSED(events))
{
	static char ev_buf[8 *(sizeof(struct inotify_event) + NAME_MAX + 1)];
	static char cond[MAX_ARG_LEN];

	struct inotify_event *ev;
	ssize_t sz, len;
	char *basename;
	svc_t *svc;

	sz = read(fd, ev_buf, sizeof(ev_buf));
	if (sz <= 0) {
		_pe("invalid inotify event\n");
		return;
	}

	for (ev = (void *)ev_buf; sz > sizeof(*ev);
	     len = sizeof(*ev) + ev->len, ev = (void *)ev + len, sz -= len) {
	     /* ev = (void *)(ev + 1) + ev->len, sz -= sizeof(*ev) + ev->len) { */
		if (!ev->mask || !strstr(ev->name, ".pid"))
			continue;

		basename = strtok(ev->name, ".");
		svc = svc_find_by_nameid(basename, 1);
		if (!svc)
			continue;

		/* TODO FIXME XXX WKZ check that pid is controlled by finit */
		
		_d("%s: match %s", basename, svc->cmd);
		snprintf(cond, sizeof(cond), "svc%s", svc->cmd);
		if (ev->mask & (IN_CREATE | IN_ATTRIB))
			cond_set(cond);
		else if (ev->mask & IN_DELETE)
			cond_clear(cond);
	}
}

static void pidfile_reconf(void *_null)
{
	static char name[MAX_ARG_LEN];

	svc_t *svc;
	(void)(_null);

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->state == SVC_RUNNING_STATE && !svc_is_changed(svc)) {
			snprintf(name, MAX_ARG_LEN, "svc%s", svc->cmd);
			cond_set_path(cond_path(name), COND_ON);
		}
	}
}

static void pidfile_init (void *arg)
{
	struct context *ctx = arg;

	ctx->wd = inotify_add_watch(ctx->fd, _PATH_VARRUN,
				    IN_CREATE | IN_ATTRIB | IN_DELETE);
	if (ctx->wd < 0) {
		_pe("inotify_add_watch()");
		close(ctx->fd);
		return;
	}

	_d("pidfile monitor active");
}

static struct context pidfile_ctx;

static plugin_t plugin = {
	.hook[HOOK_BASEFS_UP]  = { .arg = &pidfile_ctx, .cb = pidfile_init },
	.hook[HOOK_SVC_RECONF] = { .cb = pidfile_reconf },
	.io = {
		.cb    = pidfile_callback,
		.flags = PLUGIN_IO_READ,
	},
};

PLUGIN_INIT(plugin_init)
{
	pidfile_ctx.fd = inotify_init();
	if (pidfile_ctx.fd < 0) {
		_pe("inotify_init()");
		return;
	}

	plugin.io.fd = pidfile_ctx.fd;
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	inotify_rm_watch(pidfile_ctx.fd, pidfile_ctx.wd);
	close(pidfile_ctx.fd);
	
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
