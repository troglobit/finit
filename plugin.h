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

#ifndef FINIT_PLUGIN_H_
#define FINIT_PLUGIN_H_

#include <poll.h>
#include <sys/queue.h>		/* BSD sys/queue.h API */
#include "svc.h"

#define PLUGIN_IO_READ  POLLIN
#define PLUGIN_IO_WRITE POLLOUT

#define PLUGIN_INIT(x) static void __attribute__ ((constructor)) x(void)
#define PLUGIN_EXIT(x) static void __attribute__ ((destructor))  x(void)

#define PLUGIN_ITERATOR(x)  LIST_FOREACH(x, &plugins, link)

/*
 * Predefined hook points for easier plugin debugging 
 */
typedef enum {
	HOOK_BASEFS_UP = 0,
	HOOK_NETWORK_UP,
	HOOK_SYSTEM_UP,
	HOOK_SVC_UP,
	HOOK_SHUTDOWN,
	HOOK_MAX_NUM
} hook_point_t;

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
	LIST_ENTRY(plugin) link;

	/* Plugin name, defaults to plugin path if unset.
	 * NOTE: Must be same as @cmd for service plugins! */
	char *name;

	/* Service callback to be called once per lap of runloop. */
	struct {
		/* Private */
		int         id;       /* Service ID# to match this service plugin against, set on installation. */

		/* Public */
		int         dynamic;  /* Reload (SIGHUP) on dynamic event?  Set by plugin. */
		svc_cmd_t (*cb)(svc_t *svc, int event, void *event_arg);
	} svc;

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
} plugin_t;

/* Public plugin API */
int plugin_register   (plugin_t *plugin);
int plugin_unregister (plugin_t *plugin);

#endif	/* FINIT_PLUGIN_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
