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

#ifndef FINIT_SVC_H_
#define FINIT_SVC_H_

#include <sys/ipc.h>		/* IPC_CREAT */
#include <sys/resource.h>
#include <sys/types.h>		/* pid_t */
#include <lite/lite.h>
#include <lite/queue.h>		/* BSD sys/queue.h API */

#include "inetd.h"
#include "helpers.h"

typedef int svc_cmd_t;

typedef enum {
	SVC_TYPE_FREE       = 0,	/* Free to allocate */
	SVC_TYPE_SERVICE    = 1,	/* Monitored, will be respawned */
	SVC_TYPE_TASK       = 2,	/* One-shot, runs in parallell */
	SVC_TYPE_RUN        = 4,	/* Like task, but wait for completion */
	SVC_TYPE_INETD      = 8,	/* Classic inetd service */
	SVC_TYPE_INETD_CONN = 16,	/* Single inetd connection */
} svc_type_t;

#define SVC_TYPE_ANY          (-1)
#define SVC_TYPE_RUNTASK      (6)

typedef enum {
	SVC_HALTED_STATE = 0,	/* Not allowed in runlevel, or not enabled. */
	SVC_DONE_STATE,		/* Task/Run job has been run */
	SVC_STOPPING_STATE,	/* Waiting to collect the child process */
	SVC_WAITING_STATE,	/* Condition is in flux, process SIGSTOPed */
	SVC_READY_STATE,	/* Enabled but condition not satisfied */
	SVC_RUNNING_STATE,	/* Process running */
} svc_state_t;

typedef enum {
	SVC_BLOCK_NONE = 0,
	SVC_BLOCK_MISSING,
	SVC_BLOCK_CRASHING,
	SVC_BLOCK_USER,
	SVC_BLOCK_BUSY,
	SVC_BLOCK_RESTARTING,
} svc_block_t;

#define MAX_ARG_LEN      64
#define MAX_STR_LEN      64
#define MAX_COND_LEN     (MAX_ARG_LEN * 3)
#define MAX_USER_LEN     16
#define MAX_NUM_FDS      64	     /* Max number of I/O plugins */
#define MAX_NUM_SVC_ARGS 32

/*
 * Default enable for all services, can be stopped by means
 * of issuing an initctl call. E.g.
 *
 *   initctl <stop|start|restart> service
 */
typedef struct svc {
	TAILQ_ENTRY(svc) link;

	/* Instance specifics */
	int            job, id;	       /* JOB:ID */

	/* Limits and scoping */
	struct rlimit  rlimit[RLIMIT_NLIMITS];

	/* Service details */
	pid_t	       pid;
	const svc_state_t state;       /* Paused, Reloading, Restart, Running, ... */
	svc_type_t     type;	       /* Service, run, task, inetd, ... */
	struct timeval mtime;          /* Modification time for .conf from /etc/finit.d/ */
	const int      dirty;	       /* Set if old mtime != new mtime  => reloaded,
					* or -1 when marked for removal */
	int            starting;       /* ... waiting for pidfile to be re-asserted */
	int	       runlevels;
	int            sighup;	       /* This service supports SIGHUP :) */
	svc_block_t    block;	       /* Reason that this service is currently stopped */
	char           cond[MAX_COND_LEN];

	/* Counters */
	char           once;	       /* run/task, (at least) once per runlevel */
	const char     restart_cnt;    /* Incremented for each restart by service monitor. */

	/* For inetd services */
	inetd_t        inetd;
	int            stdin_fd;

	/* Set for services we need to redirect stdout/stderr to syslog */
	int            log;

	/* Identity */
	char	       username[MAX_USER_LEN];
	char	       group[MAX_USER_LEN];

	/* Command, arguments and service description */
	char	       cmd[MAX_ARG_LEN];
	char	       args[MAX_NUM_SVC_ARGS][MAX_ARG_LEN];
	char	       desc[MAX_STR_LEN];

	/*
	 * Used to forcefully kill services that won't shutdown on
	 * termination and to delay restarts of crashing services.
	 */
	uev_t          timer;
	void           (*timer_cb)(struct svc *svc);
} svc_t;

svc_t      *svc_new                (char *cmd, int id, int type);
int	    svc_del	           (svc_t *svc);

svc_t	   *svc_find	           (char *cmd, int id);
svc_t	   *svc_find_by_pid        (pid_t pid);
svc_t	   *svc_find_by_jobid      (int job, int id);
svc_t	   *svc_find_by_nameid     (char *name, int id);

svc_t	   *svc_iterator	   (svc_t *iter);
svc_t	   *svc_inetd_iterator     (svc_t *iter);
svc_t	   *svc_dynamic_iterator   (svc_t *iter);
svc_t	   *svc_named_iterator     (svc_t *iter, char *cmd);
svc_t      *svc_job_iterator       (svc_t *iter, int job);

void	    svc_foreach	           (void (*cb)(svc_t *));
void        svc_foreach_type       (int types, void (*cb)(svc_t *));

svc_t	   *svc_stop_completed	   (void);

void	    svc_mark_dynamic       (void);
void	    svc_check_dirty        (svc_t *svc, struct timeval *mtime);
void	    svc_mark_dirty         (svc_t *svc);
void	    svc_mark_clean         (svc_t *svc);
void	    svc_clean_dynamic      (void (*cb)(svc_t *));
int	    svc_clean_bootstrap    (svc_t *svc);
void	    svc_prune_bootstrap	   (void);

int         svc_enabled            (svc_t *svc);
int         svc_next_id            (char  *cmd);
int         svc_is_unique          (svc_t *svc);

int         svc_parse_jobstr       (char *str, size_t len, int (*found)(svc_t *), int (not_found)(char *, int));

static inline int svc_in_runlevel  (svc_t *svc, int runlevel) { return svc && ISSET(svc->runlevels, runlevel); }
static inline int svc_has_sighup   (svc_t *svc) { return svc &&  0 != svc->sighup; }

static inline void svc_starting    (svc_t *svc) { svc->starting = 1;         }
static inline void svc_started     (svc_t *svc) { svc->starting = 0;         }
static inline int  svc_is_starting (svc_t *svc) { return 0 != svc->starting; }

static inline int svc_is_dynamic   (svc_t *svc) { return svc &&  0 != svc->mtime.tv_sec; }
static inline int svc_is_removed   (svc_t *svc) { return svc && -1 == svc->dirty; }
static inline int svc_is_changed   (svc_t *svc) { return svc &&  0 != svc->dirty; }
static inline int svc_is_updated   (svc_t *svc) { return svc &&  1 == svc->dirty; }

static inline int svc_is_inetd     (svc_t *svc) { return svc && SVC_TYPE_INETD      == svc->type; }
static inline int svc_is_inetd_conn(svc_t *svc) { return svc && SVC_TYPE_INETD_CONN == svc->type; }
static inline int svc_is_daemon    (svc_t *svc) { return svc && SVC_TYPE_SERVICE    == svc->type; }
static inline int svc_is_runtask   (svc_t *svc) { return svc && (SVC_TYPE_RUNTASK & svc->type);   }

static inline int  svc_is_blocked  (svc_t *svc) { return svc->block != SVC_BLOCK_NONE; }
static inline int  svc_is_busy     (svc_t *svc) { return svc->block == SVC_BLOCK_BUSY; }
static inline void svc_unblock     (svc_t *svc) { svc->block = SVC_BLOCK_NONE; }
#define            svc_start(svc)  svc_unblock(svc)
static inline void svc_stop        (svc_t *svc) { svc->block = SVC_BLOCK_USER; }
static inline void svc_busy        (svc_t *svc) { svc->block = SVC_BLOCK_BUSY; }
static inline void svc_missing     (svc_t *svc) { svc->block = SVC_BLOCK_MISSING; }
static inline void svc_restarting  (svc_t *svc) { svc->block = SVC_BLOCK_RESTARTING; }
static inline void svc_crashing    (svc_t *svc) { svc->block = SVC_BLOCK_CRASHING; }

static inline char *svc_status(svc_t *svc)
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

static inline const char *svc_dirtystr(svc_t *svc)
{
	if (svc_is_removed(svc))
		return "removed";
	if (svc_is_updated(svc))
		return "updated";
	if (svc_is_changed(svc))
		return "UNKNOWN";

	return "clean";
}

#endif /* FINIT_SVC_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
