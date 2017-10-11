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

#include <err.h>
#include <ctype.h>		/* isdigit() */
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>

#include "finit.h"
#include "svc.h"
#include "helpers.h"
#include "util.h"

/* Each svc_t needs a unique job# */
static int jobcounter = 1;
static svc_t *workaround = NULL;

static svc_t *__connect_shm(void)
{
	svc_t *list;

	list = finit_svc_connect();
	if (!list) {
		if (workaround)
			return workaround;

		/* Linux not built with CONFIG_SYSVIPC, or libc does not support shmat()/shmget() */
		if (ENOSYS == errno)
			warn("Kernel does support SYSV shmat() IPC, error %d", errno);

		/* Try to prevent PID 1 from aborting, issue #81 */
		if (getpid() == 1) {
			warnx("Implementing PID 1 workaround, initctl tool will not work ...");
			if (!workaround)
				workaround = calloc(MAX_NUM_SVC, sizeof(svc_t));

			list = workaround;
		}
	}

	if (!list) {
		warn("Failed connecting to shared memory, error %d", errno);
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
			char *desc;

			memset(svc, 0, sizeof(*svc));
			svc->type = type;
			svc->job  = job;
			svc->id   = id;
			strlcpy(svc->cmd, cmd, sizeof(svc->cmd));

			/* Default description, if missing */
			desc = rindex(cmd, '/');
			if (desc)
				desc++;
			else
				desc = cmd;
			strlcpy(svc->desc, desc, sizeof(svc->desc));

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
		if (svc->mtime.tv_sec)
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
 * svc_foreach_type - Run a callback for each matching type
 * @types: Mask of service types
 * @cb:    Callback to run for each matching type
 */
void svc_foreach_type(int types, void (*cb)(svc_t *))
{
	svc_t *svc;

	if (!cb)
		return;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!(svc->type & types))
			continue;

		cb(svc);
	}
}


/**
 * svc_stop_completed - Have all stopped services been collected?
 *
 * Returns:
 * %NULL if all stopped services have been collected, otherwise a
 * pointer to the first svc_t waiting to be collected.
 */
svc_t *svc_stop_completed(void)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->state == SVC_STOPPING_STATE)
			return svc;
	}

	return NULL;
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
		if (svc->id == id && !strncmp(svc->cmd, cmd, strlen(svc->cmd)))
			return svc;
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
		*((int *)&svc->dirty) = -1;
		svc = svc_dynamic_iterator(0);
	}
}

void svc_mark_dirty(svc_t *svc)
{
	*((int *)&svc->dirty) = 1;
}

void svc_mark_clean(svc_t *svc)
{
	*((int *)&svc->dirty) = 0;
}

void svc_check_dirty(svc_t *svc, struct timeval *mtime)
{
	if (mtime && timercmp(&svc->mtime, mtime, !=))
		svc_mark_dirty(svc);
	else
		svc_mark_clean(svc);

	svc->mtime.tv_sec = mtime ? mtime->tv_sec : 0;
	svc->mtime.tv_usec = mtime ? mtime->tv_usec : 0;
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
		if (svc->dirty == -1 && cb) {
			cb(svc);
			svc_mark_clean(svc);
		}
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
			return "crashed";

		case SVC_BLOCK_USER:
			return "stopped";

		case SVC_BLOCK_BUSY:
			return "busy";

		case SVC_BLOCK_RESTARTING:
			return "restart";
		}

	case SVC_DONE_STATE:
		return "done";

	case SVC_STOPPING_STATE:
		switch (svc->type) {
		case SVC_TYPE_INETD_CONN:
		case SVC_TYPE_RUN:
		case SVC_TYPE_TASK:
			return "active";

		default:
			return "stopping";
		}

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

const char *svc_dirtystr(svc_t *svc)
{
	if (svc_is_removed(svc))
		return "removed";
	if (svc_is_updated(svc))
		return "updated";
	if (svc_is_changed(svc))
		return "UNKNOWN";

	return "clean";
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

/*
 * Used by api.c (to start/stop/restart) and initctl.c (for input validation)
 */
int svc_parse_jobstr(char *str, size_t len, int (*found)(svc_t *), int (not_found)(char *, int))
{
	int result = 0;
	char *input, *token, *pos;

	input = sanitize(str, len);
	if (!input)
		return -1;

	token = strtok_r(input, " ", &pos);
	while (token) {
		int id = 1;
		svc_t *svc;
		char *ptr = strchr(token, ':');

		if (isdigit(token[0])) {
			int job = atonum(token);

			if (!ptr) {
				svc = svc_job_iterator(1, job);
				if (!svc && not_found)
					result += not_found(NULL, job);

				while (svc) {
					if (found)
						result += found(svc);
					svc = svc_job_iterator(0, job);
				}
			} else {
				*ptr++ = 0;
				id  = atonum(ptr);
				job = atonum(token);

				svc = svc_find_by_jobid(job, id);
				if (!svc && not_found)
					result += not_found(token, id);
				else if (found)
					result += found(svc);
			}
		} else {
			if (!ptr) {
				svc = svc_named_iterator(1, token);
				if (!svc && not_found)
					result += not_found(token, id);

				while (svc) {
					if (found)
						result += found(svc);
					svc = svc_named_iterator(0, token);
				}
			} else {
				*ptr++ = 0;
				id  = atonum(ptr);

				svc = svc_find_by_nameid(token, id);
				if (!svc && not_found)
					result += not_found(token, id);
				else if (found)
					result += found(svc);
			}
		}

		token = strtok_r(NULL, " ", &pos);
	}

	return result;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
