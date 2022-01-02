/* Low-level service primitives and generic API for managing svc_t structures
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2021  Joachim Wiberg <troglobit@gmail.com>
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
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#ifdef _LIBITE_LITE
# include <libite/queue.h>	/* BSD sys/queue.h API */
#else
# include <lite/queue.h>	/* BSD sys/queue.h API */
#endif

#include "finit.h"
#include "svc.h"
#include "helpers.h"
#include "pid.h"
#include "util.h"
#include "cond.h"
#include "schedule.h"

/* Each svc_t needs a unique job# */
static int jobcounter = 1;
static TAILQ_HEAD(, svc) svc_list = TAILQ_HEAD_INITIALIZER(svc_list);
static TAILQ_HEAD(, svc) gc_list  = TAILQ_HEAD_INITIALIZER(gc_list);

/*
 * Before gc removal of svc, make sure we don't clear an active
 * condition of a new instance of the svc.
 */
static void maybe_clear_cond(svc_t *svc)
{
	char ident[MAX_IDENT_LEN];
	char cond[MAX_COND_LEN];
	svc_t *iter = NULL;
	svc_t *s;

	mkcond(svc, cond, sizeof(cond));
	svc_ident(svc, ident, sizeof(ident));

	for (s = svc_iterator(&iter, 1); s; s = svc_iterator(&iter, 0)) {
		char c[MAX_COND_LEN];

		mkcond(s, c, sizeof(c));
		if (!string_compare(cond, c))
			continue;

		_d("Not clearing cond %s from gc svc %s, provided by new active service %s",
		   cond, ident, svc_ident(s, NULL, 0));
		return;
	}

	_d("Cleaning out %s, clearing any conditions ...", svc->name);
	cond_clear(mkcond(svc, cond, sizeof(cond)));
}

static void svc_gc(void *arg)
{
	struct timespec now;
	struct wq *work = (struct wq *)arg;
	svc_t *svc, *next;

	clock_gettime(CLOCK_MONOTONIC_COARSE, &now);
	TAILQ_FOREACH_SAFE(svc, &gc_list, link, next) {
		int msec;

		msec  = (now.tv_sec  - svc->gc.tv_sec)  * 1000 +
			(now.tv_nsec - svc->gc.tv_nsec) / 1000000;
		if (msec < SVC_TERM_TIMEOUT)
			continue;

		TAILQ_REMOVE(&gc_list, svc, link);
		maybe_clear_cond(svc);
		free(svc);
	}

	if (!TAILQ_EMPTY(&gc_list))
		schedule_work(work);
}

/**
 * svc_new - Create a new service
 * @cmd:  External program to call
 * @id:   Instance id
 * @type: Service type, one of service, task, run
 *
 * Returns:
 * A pointer to a new &svc_t object, or %NULL if out of empty slots.
 */
svc_t *svc_new(char *cmd, char *id, int type)
{
	int job = -1;
	svc_t *svc, *iter = NULL;

	/* Find first job n:o if registering multiple instances */
	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (cmd && !strcmp(svc->cmd, cmd)) {
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
	if (id && id[0])
		strlcpy(svc->id, id, sizeof(svc->id));
	if (cmd)
		strlcpy(svc->cmd, cmd, sizeof(svc->cmd));

	/* Default description, if missing */
	strlcpy(svc->desc, svc->name, sizeof(svc->desc));

	/* Default HALT signal to send */
	if (svc_is_tty(svc))
		svc->sighalt = SIGHUP;
	else
		svc->sighalt = SIGTERM;

	/* Default delay between SIGTERM and SIGKILL */
	svc->killdelay = SVC_TERM_TIMEOUT;

	TAILQ_INSERT_TAIL(&svc_list, svc, link);

	return svc;
}

static struct wq work = {
	.cb    = svc_gc,
	.delay = SVC_TERM_TIMEOUT
};

/**
 * svc_del - Mark a service object for deletion
 * @svc: Pointer to an &svc_t object
 *
 * Returns:
 * Always succeeds, returning POSIX OK(0).
 */
int svc_del(svc_t *svc)
{
	TAILQ_REMOVE(&svc_list, svc, link);
	TAILQ_INSERT_TAIL(&gc_list, svc, link);

	clock_gettime(CLOCK_MONOTONIC_COARSE, &svc->gc);
	schedule_work(&work);

	return 0;
}

/**
 * svc_validate - Check if service asserts same condition as another service
 * @svc: Pointer to an &svc_t object
 *
 * Logs to syslog if there is a name clash with existing services.
 */
void svc_validate(svc_t *svc)
{
	char ident[MAX_IDENT_LEN];
	char cond[MAX_COND_LEN];
	svc_t *iter = NULL;
	svc_t *s;

	mkcond(svc, cond, sizeof(cond));
	svc_ident(svc, ident, sizeof(ident));

	for (s = svc_iterator(&iter, 1); s; s = svc_iterator(&iter, 0)) {
		char c[MAX_COND_LEN];

		if (s->removed)
			continue;
		if (s == svc)
			continue;

		mkcond(s, c, sizeof(c));
		if (!string_compare(cond, c))
			continue;

		logit(LOG_WARNING, "%s (%s) asserts the same condition as %s (%s) => %s",
		      svc->cmd, ident, s->cmd, svc_ident(s, NULL, 0), c);
	}
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
		char *name = svc->name;

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
void svc_foreach(int (*cb)(svc_t *))
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
void svc_foreach_type(int types, int (*cb)(svc_t *))
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
		if (svc->state == SVC_STOPPING_STATE && svc->pid > 1)
			return svc;
	}

	return NULL;
}

/**
 * svc_find - Find a service object by its full path name
 * @cmd: Full path name, e.g., /sbin/syslogd
 * @id:  Optional instance id
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find(char *cmd, char *id)
{
	svc_t *svc, *iter = NULL;

	if (!id)
		id = "";

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!strcmp(svc->cmd, cmd) && !strcmp(svc->id, id))
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
 * @id:  Optional instance id
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_jobid(int job, char *id)
{
	svc_t *svc, *iter = NULL;

	if (!id)
		id = "";

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (svc->job == job && !strcmp(svc->id, id))
			return svc;
	}

	return NULL;
}

/**
 * svc_find_by_nameid - Find an service object by its basename:ID
 * @name: Process name to match
 * @id:   Optional instance id
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_nameid(char *name, char *id)
{
	svc_t *svc, *iter = NULL;

	if (!id)
		id = "";

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!strcmp(svc->id, id) && !strcmp(name, svc->name))
			return svc;
	}

	return NULL;
}


svc_t *svc_find_by_tty(char *dev)
{
	svc_t *svc, *iter = NULL;

	/* rescue (notty) shells have no device node */
	if (!dev)
		return NULL;

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!svc_is_tty(svc))
			continue;

		if (!strcmp(dev, svc->dev))
			return svc;
	}

	return NULL;
}

/**
 * svc_find_by_plidfile - Find a service by its PID file
 * @fn: PID file, can be absolute path or relative to /run
 *
 * This function is primarily used by the pidfile plugin to track the
 * conditions and dependencies of services.
 *
 * Note, the PID file is assumed to exist so the contents can be
 * compared to the the PID of a located service.  There may be several
 * services, in many runlevels, that use the same name for the PID file.
 * So we won't know for sure until the PID is compared.
 *
 * Returns:
 * A pointer to an &svc_t object, or %NULL if not found.
 */
svc_t *svc_find_by_pidfile(char *fn)
{
	svc_t *svc, *iter = NULL;
	pid_t pid;

	pid = pid_file_read(fn);
	if (pid <= 0) {
		for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
			int v = 0;

			if (svc->pidfile[0] == '!')
				v = 1;
			if (strcmp(&svc->pidfile[v], fn))
				continue;

			return svc;
		}

		return NULL;
	}

	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (svc->pid != pid)
			continue;

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
		if (svc->protect)
			continue;

		*((int *)&svc->removed) = 1;
	}
}

void svc_mark_dirty(svc_t *svc)
{
	*((int *)&svc->dirty) = 1;
}

void svc_mark_clean(svc_t *svc)
{
	*((int *)&svc->dirty) = 0;
	*((int *)&svc->args_dirty) = 0;
}

void svc_enable(svc_t *svc)
{
	*((int *)&svc->removed) = 0;
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
		if (svc->removed && cb)
			cb(svc);
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

	if (svc_is_missing(svc))
		return 0;

	if (svc_is_tty(svc) && bootstrap)
		return 0;

	return 1;
}

/* break up "job:id job:id job:id ..." into several "job:id" tokens */
static char *tokstr(char *str, size_t len)
{
	static char   *cur = NULL;
	static size_t  pos = 0;
	char *token;

	if (!cur && !str)
		return NULL;

	if (str) {
		cur = str;
		pos = 0;
	}

	if (pos >= len)
		return NULL;

	token = &cur[pos];
	while (pos < len) {
		if (isspace(cur[pos])) {
			cur[pos++] = 0;
			break;
		}
		pos++;
	}

	return token;
}

/*
 * Used by api.c (to start/stop/restart) and initctl.c (for input validation)
 */
int svc_parse_jobstr(char *str, size_t len, int (*found)(svc_t *), int (not_found)(char *, char *))
{
	char *input, *token;
	int result = 0;

	_d("Got str:'%s'", str);
	input = tokstr(str, len);
	while (input) {
		char *id = NULL;
		svc_t *svc, *iter = NULL;
		char *ptr;

		_d("Got token:'%s'", input);
		token = sanitize(input, strlen(input) + 1);
		if (!token) {
			_d("Sanitation of token:'%s' failed", input);
			goto next;
		}
		ptr = strchr(token, ':');

		if (isdigit(token[0])) {
			char *ep;
			long job = 0;

			errno = 0;
			job = strtol(token, &ep, 10);
			if ((errno == ERANGE && (job == LONG_MAX || job == LONG_MIN)) ||
			    (errno != 0 && job == 0) ||
			    (token == ep)) {
				result++;
				continue;
			}

			if (!ptr) {
				svc = svc_job_iterator(&iter, 1, job);
				if (!svc && not_found)
					result += not_found(NULL, token);

				while (svc) {
					if (found)
						result += found(svc);
					svc = svc_job_iterator(&iter, 0, job);
				}
			} else {
				*ptr++ = 0;
				id  = ptr;

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
				id  = ptr;

				svc = svc_find_by_nameid(token, id);
				if (!svc && not_found)
					result += not_found(token, id);
				else if (found)
					result += found(svc);
			}
		}

	next:
		input = tokstr(NULL, len);
	}

	return result;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
