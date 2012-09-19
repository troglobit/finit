/* Generic API for managing svc_t structures
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
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

#include <string.h>

#include "finit.h"
#include "svc.h"

/* The registered services are kept in shared memory for easy read-only access
 * by 3rd party APIs.  Mostly because of the ability to, from the outside, query
 * liveness state of all daemons that should run. */
static int svc_counter = 0;		/* Number of registered services to monitor. */
static svc_t *services = NULL;          /* List of registered services, in shared memory. */

/* Connect to/create shared memory area of registered servies */
static void __connect_shm(void)
{
	if (!services) {
		services = finit_connect_shm();
		if (!services) {
			/* This should never happen, but if it does we're probably
			 * knee-deep in more serious problems already... */
			_e("Failed setting up shared memory for service monitor.\n");
			abort();
		}
	}
}

svc_t *svc_new_service (void)
{
	svc_t *svc = NULL;

	__connect_shm();
	if (svc_counter < MAX_NUM_SVC) {
		svc = &services[svc_counter++];
		memset(svc, 0, sizeof(*svc));
	}

	return svc;
}

svc_t *svc_find_by_id (int id)
{
	__connect_shm();
	if (id >= svc_counter)
		return NULL;

	return &services[id];
}

svc_t *svc_find_by_name (char *name)
{
	int i;

	__connect_shm();
	for (i = 0; i < svc_counter; i++) {
		svc_t *svc = &services[i];

		if (!strncmp(name, svc->cmd, strlen(svc->cmd))) {
			_d("Found a matching svc for %s", name);
			return svc;
		}
	}

	return NULL;
}

/* Iterates over all registered services. Get first svc_t by setting @restart */
svc_t *svc_iterator (int restart)
{
	static int id = 0;

	__connect_shm();
	if (restart)
		id = 0;

	if (id >= svc_counter)
		return NULL;

	return &services[id++];
}

/**
 * Local Variables:
 *  compile-command: "gcc -g -DUNITTEST -o unittest svc.c"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
