/* Plugin based services architecture for finit
 *
 * Copyright (c) 2012-2017  Joachim Nilsson <troglobit@gmail.com>
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

#ifndef FINIT_PLUGIN_H_
#define FINIT_PLUGIN_H_

#include <uev/uev.h>
#include <lite/queue.h>		/* BSD sys/queue.h API */

#include "svc.h"

#define PLUGIN_DEP_MAX  10

/*
 * Event flags for I/O plugins
 */
#define PLUGIN_IO_ERROR UEV_ERROR
#define PLUGIN_IO_READ  UEV_READ
#define PLUGIN_IO_WRITE UEV_WRITE
#define PLUGIN_IO_PRI   UEV_PRI
#define PLUGIN_IO_HUP   UEV_HUP
#define PLUGIN_IO_RDHUP UEV_RDHUP

#define PLUGIN_INIT(x) static void __attribute__ ((constructor)) x(void)
#define PLUGIN_EXIT(x) static void __attribute__ ((destructor))  x(void)

#define PLUGIN_ITERATOR(x, tmp) TAILQ_FOREACH_SAFE(x, &plugins, link, tmp)

/*
 * Predefined hook points and corresponding conditions in Finit,
 * for use by plugins and scripts.  Recommended to use the task
 * or run stanzas:  task <hook/mount/error> /bin/rescue.sh
 *
 * Some of the below hooks cannot (currently) be realized as conditions.
 * The idea was to rename them action scripts, with optional argument,
 * according to the following scheme:
 *
 * - HOOK_SVC_RECONF      :: action/svc/reconf
 * - HOOK_SVC_LOST        :: action/svc/lost
 * - HOOK_SVC_START       :: action/svc/start
 * - HOOK_RUNLEVEL_CHANGE :: action/sys/runlevel
 *
 * However, the implementation did not turn out to be stable enough for
 * general release, so it was pulled.
 */
#define HOOK_TYPES {						\
	/* Bootstrap hooks, runlevel [S] */			\
	CHOOSE(HOOK_BANNER = 0,      "hook/sys/banner"),	\
	CHOOSE(HOOK_ROOTFS_UP,       "hook/mount/root"),	\
	CHOOSE(HOOK_MOUNT_ERROR,     "hook/mount/error"),	\
	CHOOSE(HOOK_MOUNT_POST,      "hook/mount/post"),	\
	CHOOSE(HOOK_BASEFS_UP,       "hook/mount/all"),		\
	CHOOSE(HOOK_NETWORK_UP,      "hook/net/up"),		\
	CHOOSE(HOOK_SVC_UP,          "hook/svc/up"),		\
	CHOOSE(HOOK_SYSTEM_UP,       "hook/sys/up"),		\
								\
	/* Runtime hooks, runlevel [S1-9] */			\
	CHOOSE(HOOK_SVC_RECONF,      "nop"),			\
	CHOOSE(HOOK_SVC_LOST,        "nop"),			\
	CHOOSE(HOOK_SVC_START,       "nop"),			\
	CHOOSE(HOOK_RUNLEVEL_CHANGE, "nop"),			\
								\
	/* Shutdown hooks, runlevel [06] */			\
	CHOOSE(HOOK_SHUTDOWN,        "hook/sys/shutdown"),	\
	CHOOSE(HOOK_MAX_NUM,         "nop")			\
}

#define CHOOSE(x, y) x
typedef enum HOOK_TYPES hook_point_t;
#undef CHOOSE

/**
 * plugin_t - Finit &plugin_t object
 * @link: BSD sys/queue.h linked list node
 * @name: Plugin name, or identifier to match against a &svc_t object
 * @svc:  Service callback for a loaded &svc_t object
 * @hook: Hook callback definitions
 * @io:   I/O hook callback
 *
 * To setup an &svc_t object callback for a service monitor the @name
 * must match the @svc_t @cmd exactly for them to "pair".
 *
 * A &plugin_t object may contain a service callback, several hooks
 * and/or an I/O callback as well. This way all critical extensions
 * to finit can fit in one single plugin, if needed.
 *
 * The "dynamic events" discussed in the svc callback is for external
 * service plugins to implement.  However, it can be anything that 
 * can cause a service to need to SIGHUP at runtime.  E.g., acquiring
 * a DHCP lease, or an interface going UP/DOWN. The event itself can
 * be passed as an integer @event and any optional argument @event_arg
 *
 * It is up to the external service plugin to track these events and
 * relay them to each @dynamic service plugins' callback.  I.e., to
 * all those with the dynamic flag set.
 */
typedef struct plugin {
	/* BSD sys/queue.h linked list node. */
	TAILQ_ENTRY(plugin) link;

	/* Handle to be sent to dlclose() when plugins is unloaded. */
	void *handle;

	/* Event loop handler, used internally by Finit */
	uev_t watcher;

	/* Plugin name, defaults to basename of plugin path if unset.
	 * NOTE: Must match cmd for services or inetd plugins! */
	char *name;

	/* List of hook callbacks. */
	struct {
		void  *arg;      /* Optional argument to callback func. */
		void (*cb)(void *arg);
	} hook[HOOK_MAX_NUM];

	/* I/O Plugin */
	struct {
		int    fd, flags; /* 1:READ, 2:WRITE */
		void  *arg;
		void (*cb)(void *arg, int fd, int events);
	} io;

	/* Inetd Plugin, stdio used as client socket.
	 * @type argument will be either SOCK_DGRAM or SOCK_STREAM */
	struct {
		int (*cmd)(int type);
	} inetd;

	char *depends[PLUGIN_DEP_MAX]; /* List of other .name's this depends on. */
} plugin_t;

/* Public plugin API */
int plugin_io_init    (plugin_t *plugin);

int plugin_register   (plugin_t *plugin);
int plugin_unregister (plugin_t *plugin);

/* Helper API */
plugin_t *plugin_find (char *name);

#endif	/* FINIT_PLUGIN_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
