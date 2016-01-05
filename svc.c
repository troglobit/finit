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
#include "libite/lite.h"

#include "finit.h"
#include "svc.h"
#include "helpers.h"

/* Each svc_t needs a unique job# */
static int jobcounter = 1;

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
 * @cmd:  External program to call, or 'internal' for internal inetd services
 * @id:   Instance id
 * @type: Service type, one of service, task, run or inetd
 *
 * Returns:
 * A pointer to a new &svc_t object, or %NULL if out of empty slots.
 */
svc_t *svc_new(char *cmd, int id, int type)
{
	int i, job = -1;
	svc_t *svc, *list = __connect_shm();

	/* Find first job n:o if registering multiple instances */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!strcmp(svc->cmd, cmd)) {
			job = svc->job;
			break;
		}
	}
	if (job == -1)
		job = jobcounter++;

	for (i = 0; i < MAX_NUM_SVC; i++) {
		svc_t *svc = &list[i];

		if (svc->type == SVC_TYPE_FREE) {
			memset(svc, 0, sizeof(*svc));
			svc->type = type;
			svc->job  = job;
			svc->id   = id;
			strlcpy(svc->cmd, cmd, sizeof(svc->cmd));

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
	svc_t *svc;

	for (svc = svc_iterator(first); svc; svc = svc_iterator(0)) {
		if (svc_is_inetd(svc))
			return svc;
	}

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
	svc_t *svc;

	for (svc = svc_iterator(first); svc; svc = svc_iterator(0)) {
		if (svc->mtime)
			return svc;
	}

	return NULL;
}


/**
 * svc_named_iterator - Iterates over all instances of a service.
 * @first: Get first &svc_t object, or next until end.
 * @cmd:   Service name to look for.
 *
 * Returns:
 * The first matching &svc_t when @first is set, otherwise the next
 * &svc_t instance with the same @cmd until the end when %NULL is
 * returned.
 */
svc_t *svc_named_iterator(int first, char *cmd)
{
	svc_t *svc;

	for (svc = svc_iterator(first); svc; svc = svc_iterator(0)) {
		char *name = basename(svc->cmd);

		if (!strncmp(name, cmd, strlen(name)))
			return svc;
	}

	return NULL;
}


/**
 * svc_job_iterator - Iterates over all instances of a service.
 * @first: Get first &svc_t object, or next until end.
 * @job:   Job to look for.
 *
 * Returns:
 * The first matching &svc_t when @first is set, otherwise the next
 * &svc_t instance with the same @job until the end when %NULL is
 * returned.
 */
svc_t *svc_job_iterator(int first, int job)
{
	svc_t *svc;

	for (svc = svc_iterator(first); svc; svc = svc_iterator(0)) {
		if (svc->job == job)
			return svc;
	}

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
 * @cmd: Full path name, e.g., /sbin/syslogd
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find(char *cmd, int id)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->id == id && !strncmp(svc->cmd, cmd, strlen(svc->cmd))) {
			_d("Found a matching svc for %s", cmd);
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
 * svc_find_by_jobid - Find an service object by its JOB:ID
 * @job: Job n:o
 * @id:  Instance id
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_jobid(int job, int id)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->job == job && svc->id == id)
			return svc;
	}

	return NULL;
}

/**
 * svc_find_by_nameid - Find an service object by its basename:ID
 * @name: Process name to match
 * @id:   Instance id
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_nameid(char *name, int id)
{
	char *ptr;
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		ptr = strrchr(svc->cmd, '/');
		if (ptr)
			ptr++;
		else
			ptr = svc->cmd;

		if (svc->id == id && !strcmp(name, ptr))
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

void svc_check_dirty(svc_t *svc, time_t mtime)
{
	if (svc->mtime != mtime)
		svc->dirty = 1;
	else
		svc->dirty = 0;
	svc->mtime = mtime;
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
 * svc_clean_bootstrap - Remove bootstrap-only services after boot
 * @svc: Pointer to &svc_t object
 *
 * Returns:
 * %TRUE(1) if object was bootstrap-only, otherwise %FALSE(0)
 */
int svc_clean_bootstrap(svc_t *svc)
{
	if (!ISOTHER(svc->runlevels, 0)) {
		svc->pid = 0;
		svc_del(svc);
		return 1;
	}

	return 0;
}

char *svc_status(svc_t *svc)
{
	switch (svc->state) {
	case SVC_HALTED_STATE:
		switch (svc->block) {
		case SVC_BLOCK_NONE:
			return "halted";
		case SVC_BLOCK_MISSING:
			return "missing";
		case SVC_BLOCK_CRASHING:
			return "crashing";
		case SVC_BLOCK_USER:
			return "blocked";
		}
	case SVC_DONE_STATE:
		return "done";
	case SVC_STOPPING_STATE:
		return "stopping";
	case SVC_WAITING_STATE:
		return "waiting";
	case SVC_READY_STATE:
		return "ready";
	case SVC_RUNNING_STATE:
		return "running";

	default:
		return "UNKNOWN";
	}
}

/* Same base service, return unique ID */
int svc_next_id(char *cmd)
{
	int id = 0;
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!strcmp(svc->cmd, cmd) && id < svc->id)
			id = svc->id;
	}

	return id + 1;
}

int svc_is_unique(svc_t *svc)
{
	svc_t *list = __connect_shm();
	int i, unique = 1;

	for (i = 0; i < MAX_NUM_SVC; i++) {
		svc_t *s = &list[i];

		if (svc->type == SVC_TYPE_FREE)
			continue;

		if (s == svc)
			continue;

		if (s->job == svc->job) {
			unique = 0;
			break;
		}
	}

	return unique;
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
