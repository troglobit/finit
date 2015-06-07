/* Low-level service primitives and generic API for managing svc_t structures
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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

#include <stdlib.h>

#include "finit.h"
#include "svc.h"
#include "helpers.h"


static svc_t *__connect_shm(void)
{
	svc_t *list = finit_svc_connect();

	if (!list) {
		/* This should never happen, but if it does we're probably
		 * knee-deep in more serious problems already... */
		_e("Failed connecting to shared memory, error %d: %s", errno, strerror(errno));
		abort();
	}

	return list;
}

/**
 * svc_new - Create a new service
 * @id: Instance id
 *
 * Returns:
 * A pointer to a new &svc_t object, or %NULL if out of empty slots.
 */
svc_t *svc_new(int id)
{
	int i;
	svc_t *list = __connect_shm();

	for (i = 0; i < MAX_NUM_SVC; i++) {
		svc_t *svc = &list[i];

		if (svc->type == SVC_TYPE_FREE) {
			memset(svc, 0, sizeof(*svc));
			svc->id = id;

			return svc;
		}
	}

	errno = ENOMEM;
	return NULL;
}

/**
 * svc_del - Delete a service object
 * @svc: Pointer to an &svc_t object
 *
 * Returns:
 * Always succeeds, returning POSIX OK(0).
 */
int svc_del(svc_t *svc)
{
	svc->type = SVC_TYPE_FREE;
	return 0;
}

/**
 * svc_iterator - Naive iterator over all registered services.
 * @first: Get first &svc_t object, or next until end.
 *
 * Returns:
 * The first &svc_t when @first is set, otherwise the next &svc_t until
 * the end when %NULL is returned.
 */
svc_t *svc_iterator(int first)
{
	static int i = 0;
	svc_t *list = __connect_shm();

	if (first)
		i = 0;

	while (i < MAX_NUM_SVC) {
		svc_t *svc = &list[i++];

		if (svc->type != SVC_TYPE_FREE)
			return svc;
	}

	return NULL;
}


/**
 * svc_inetd_iterator - Naive iterator over all registered inetd services.
 * @first: Get first &svc_t object, or next until end.
 *
 * Returns:
 * The first inetd &svc_t when @first is set, otherwise the next
 * inetd &svc_t until the end when %NULL is returned.
 */
svc_t *svc_inetd_iterator(int first)
{
	svc_t *svc = svc_iterator(first);

	do {
		if (svc_is_inetd(svc))
			return svc;
	} while ((svc  = svc_iterator(0)));

	return NULL;
}


/**
 * svc_dynamic_iterator - Naive iterator over all registered dynamic services.
 * @first: Get first &svc_t object, or next until end.
 *
 * Returns:
 * The first dynamically loaded &svc_t when @first is set, otherwise the
 * next dynamically loaded &svc_t until the end when %NULL is returned.
 */
svc_t *svc_dynamic_iterator(int first)
{
	svc_t *svc = svc_iterator(first);

	do {
		if (svc->mtime)
			return svc;
	} while ((svc  = svc_iterator(0)));

	return NULL;
}


/**
 * svc_foreach - Run a callback for each registered service
 * @cb: Callback to run for each service
 */
void svc_foreach(void (*cb)(svc_t *))
{
	svc_t *svc;

	if (!cb)
		return;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0))
		cb(svc);
}


/**
 * svc_foreach_dynamic - Run a callback for each registered dynamic service
 * @cb: Callback to run for each dynamic service
 */
void svc_foreach_dynamic(void (*cb)(svc_t *))
{
	svc_t *svc;

	if (!cb)
		return;

	for (svc = svc_dynamic_iterator(1); svc; svc = svc_dynamic_iterator(0))
		cb(svc);
}


/**
 * svc_find - Find a service object by its full path name
 * @path: Full path name, e.g., /sbin/syslogd
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find(char *path, int id)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->id == id && !strncmp(path, svc->cmd, strlen(svc->cmd))) {
			_d("Found a matching svc for %s", path);
			return svc;
		}
	}

	return NULL;
}

/**
 * svc_find_by_pid - Find an service object by its PID
 * @pid: Process ID to match
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_pid(pid_t pid)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->pid == pid)
			return svc;
	}

	return NULL;
}

/**
 * svc_mark_dynamic - Mark dynamically loaded services for deletion.
 *
 * This function traverses the list of known services, marking all that
 * have been loaded from /etc/finit.d/ for deletion.  If a .conf file
 * has been removed svc_cleanup_dynamic() will stop and delete the
 * service.
 *
 * This function is called from sighup_cb().
 */
void svc_mark_dynamic(void)
{
	svc_t *svc = svc_dynamic_iterator(1);

	while (svc) {
		svc->dirty = -1;
		svc = svc_dynamic_iterator(0);
	}
}

/**
 * svc_clean_dynamic - Stop and cleanup stale services removed from /etc/finit.d
 * @cb: Callback to run for each stale service
 *
 * This function traverses the list of known services to stop and clean
 * up all non-existing services previously marked by svc_mark_dynamic().
 */
void svc_clean_dynamic(void (*cb)(svc_t *))
{
	svc_t *svc = svc_dynamic_iterator(1);

	while (svc) {
		if (svc->dirty == -1 && cb)
			cb(svc);

		svc->dirty = 0;
		svc = svc_dynamic_iterator(0);
	}
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
