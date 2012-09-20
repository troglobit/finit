/* Plugin framework
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

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include <sys/queue.h>		/* BSD sys/queue.h API */

#define __unused __attribute__ ((unused))
#define PLUGIN_INIT(x) void x(void) __attribute__ ((constructor));
#define PLUGIN_EXIT(x) void x(void) __attribute__ ((destructor));

#define PLUGIN_ITERATOR(x)  LIST_FOREACH(x, &plugins, link)

/*
 * Predefined hook points for easier plugin debugging 
 */
typedef enum {
	HOOK_POST_LOAD = 0,
	HOOK_PRE_SETUP,
	HOOK_PRE_RUNLOOP,
	HOOK_EXIT_CLEANUP,
	HOOK_MAX_NUM
} hook_point_t;

typedef struct plugin {
	/* BSD sys/queue.h linked list node. */
	LIST_ENTRY(plugin) link;

	/* Optional plugin name, defaults to plugin path if unset. */
	const char *name;

	/* Service callback to be called once per lap of runloop. */
	struct {
		void *arg;	/* Optional argument to callback func. */
		void (*cb) (void *);
	} svc;

	/* List of hook callbacks. */
	struct {
		void *arg;     /* Optional argument to callback func. */
		void (*cb) (void *);
	} hook[HOOK_MAX_NUM];
} plugin_t;

/* Public plugin API */
int plugin_register   (plugin_t *plugin);
int plugin_unregister (plugin_t *plugin);

/* Private daemon API */
void run_hooks        (hook_point_t no);
void run_services     (void);
int  load_plugins     (void);

#endif	/* PLUGIN_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
