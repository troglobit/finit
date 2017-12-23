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
#include <lite/queue.h>		/* BSD sys/queue.h API */

#include "finit.h"
#include "svc.h"
#include "helpers.h"
#include "util.h"

/* Each svc_t needs a unique job# */
static int jobcounter = 1;
static TAILQ_HEAD(head, svc) svc_list = TAILQ_HEAD_INITIALIZER(svc_list);

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
	int job = -1;
	char *desc;
	svc_t *svc, *iter = NULL;

	/* Find first job n:o if registering multiple instances */
	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!strcmp(svc->cmd, cmd)) {
			job = svc->job;
			break;
		}
	}
	if (job == -1)
		job = jobcounter++;

	svc = calloc(1, sizeof(*svc));
	if (!svc)
		return NULL;

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

	TAILQ_INSERT_TAIL(&svc_list, svc, link);

	return svc;
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
	TAILQ_REMOVE(&svc_list, svc, link);
	memset(svc, 0, sizeof(*svc));
	free(svc);

	return 0;
}

/**
 * svc_iterator - Naive iterator over all registered services.
 * @iter:  Iterator, must be a valid pointer
 * @first: If set, get first &svc_t, otherwise get next
 *
 * Returns:
 * An &svc_t pointer, or %NULL when no more entries can be found.
 */
svc_t *svc_iterator(svc_t **iter, int first)
{
	svc_t *svc;

	if (!iter) {
		errno = EINVAL;
		return NULL;
	}

	if (first)
		svc = TAILQ_FIRST(&svc_list);
	else
		svc = *iter;

	if (svc)
		*iter = TAILQ_NEXT(svc, link);

	return svc;
}

/**
 * svc_inetd_iterator - Naive iterator over all registered inetd services.
 * @iter:  Iterator, must be a valid pointer
 * @first: If set, get first &svc_t, otherwise get next
 *
 * Returns:
 * The first inetd &svc_t when %NULL is given as argument, otherwise the
 * next inetd &svc_t until the end when %NULL is returned.
 */
svc_t *svc_inetd_iterator(svc_t **iter, int first)
{
	svc_t *svc;

	for (svc = svc_iterator(iter, first); svc; svc = svc_iterator(iter, 0)) {
		if (svc_is_inetd(svc))
			return svc;
	}

	return NULL;
}


/**
 * svc_named_iterator - Iterates over all instances of a service.
 * @iter:  Iterator, must be a valid pointer
 * @first: If set, get first &svc_t, otherwise get next
 * @cmd:   Service name to look for.
 *
 * Returns:
 * The first matching &svc_t when %NULL is given as argument, otherwise
 * the next &svc_t with the same @cmd name until the end when %NULL is
 * returned.
 */
svc_t *svc_named_iterator(svc_t **iter, int first, char *cmd)
{
	svc_t *svc;

	for (svc = svc_iterator(iter, first); svc; svc = svc_iterator(iter, 0)) {
		char *name = basename(svc->cmd);

		if (!strncmp(name, cmd, strlen(name)))
			return svc;
	}

	return NULL;
}


/**
 * svc_job_iterator - Iterates over all instances of a service.
 * @iter:  Iterator, must be a valid pointer
 * @first: If set, get first &svc_t, otherwise get next
 * @job:   Job to look for.
 *
 * Returns:
 * The first matching &svc_t when %NULL is given as argument, otherwise
 * the next &svc_t with the same @job ID until the end when %NULL is
 * returned.
 */
svc_t *svc_job_iterator(svc_t **iter, int first, int job)
{
	svc_t *svc;

	for (svc = svc_iterator(iter, first); svc; svc = svc_iterator(iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	if (!cb)
		return;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0))
		cb(svc);
}


/**
 * svc_foreach_type - Run a callback for each matching type
 * @types: Mask of service types
 * @cb:    Callback to run for each matching type
 */
void svc_foreach_type(int types, void (*cb)(svc_t *))
{
	svc_t *svc, *iter = NULL;

	if (!cb)
		return;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
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
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (svc->protected)
			continue;

		*((int *)&svc->dirty) = -1;
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

/**
 * svc_clean_dynamic - Stop and cleanup stale services removed from /etc/finit.d
 * @cb: Callback to run for each stale service
 *
 * This function traverses the list of known services to stop and clean
 * up all non-existing services previously marked by svc_mark_dynamic().
 */
void svc_clean_dynamic(void (*cb)(svc_t *))
{
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (svc->dirty == -1 && cb) {
			cb(svc);
			svc_mark_clean(svc);
		}
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

/**
 * svc_prune_bootstrap - Prune any remaining bootstrap tasks
 *
 * This function cleans up all bootstrap tasks that have not yet been
 * collected, or that never even ran in bootstrap.  All other instances
 * when svc_clean_bootstrap() is called are when a service is collected.
 */
void svc_prune_bootstrap(void)
{
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!svc->pid)
			svc_clean_bootstrap(svc);
	}
}

/**
 * svc_enabled - Should the service run?
 * @svc: Pointer to &svc_t object
 *
 * Returns:
 * 1, if the service is allowed to run in the current runlevel and the
 * user has not manually requested that this service should not run. 0
 * otherwise.
 */
int svc_enabled(svc_t *svc)
{
	if (!svc)
		return 0;

	if (!svc_in_runlevel(svc, runlevel))
		return 0;

	if (svc_is_removed(svc) || svc_is_blocked(svc))
		return 0;

	return 1;
}

/* Same base service, return unique ID */
int svc_next_id(char *cmd)
{
	int id = 0;
	svc_t *svc, *iter = NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!strcmp(svc->cmd, cmd) && id < svc->id)
			id = svc->id;
	}

	return id + 1;
}

int svc_is_unique(svc_t *svc)
{
	svc_t *s, *iter = NULL;
	int unique = 1;

	for (s = svc_iterator(&iter, 1); s; s = svc_iterator(&iter, 0)) {
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
		svc_t *svc, *iter = NULL;
		char *ptr = strchr(token, ':');

		if (isdigit(token[0])) {
			int job = atonum(token);

			if (!ptr) {
				svc = svc_job_iterator(&iter, 1, job);
				if (!svc && not_found)
					result += not_found(NULL, job);

				while (svc) {
					if (found)
						result += found(svc);
					svc = svc_job_iterator(&iter, 0, job);
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
				svc = svc_named_iterator(&iter, 1, token);
				if (!svc && not_found)
					result += not_found(token, id);

				while (svc) {
					if (found)
						result += found(svc);
					svc = svc_named_iterator(&iter, 0, token);
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
