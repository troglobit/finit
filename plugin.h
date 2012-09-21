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

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include <sys/queue.h>		/* BSD sys/queue.h API */

#define PLUGIN_INIT(x) void x(void) __attribute__ ((constructor));
#define PLUGIN_EXIT(x) void x(void) __attribute__ ((destructor));

#define PLUGIN_ITERATOR(x)  LIST_FOREACH(x, &plugins, link)

/*
 * Predefined hook points for easier plugin debugging 
 */
typedef enum {
	HOOK_POST_LOAD = 0,
	HOOK_PRE_SETUP,
	HOOK_POST_SIGSETUP,
	HOOK_POST_NETWORK,
	HOOK_PRE_RUNLOOP,
	HOOK_SHUTDOWN,
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
int  load_plugins     (char *path);

#endif	/* PLUGIN_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
