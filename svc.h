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

#ifndef FINIT_SVC_H_
#define FINIT_SVC_H_

#include <errno.h>
#include <string.h>		/* strerror() */
#include <sys/shm.h>		/* shmat() */
#include <sys/types.h>		/* pid_t */

#include "plugin.h"

typedef enum {
	SVC_STOP = 0,		/* Disabled */
	SVC_START,		/* Enabled */
	SVC_RELOAD		/* Enabled, needs restart */
} svc_cmd_t;

#define FINIT_SHM_ID     0x494E4954  /* "INIT", see ascii(7) */
#define MAX_ARG_LEN      64
#define MAX_STR_LEN      64
#define MAX_USER_LEN     16
#define MAX_NUM_FDS      64	     /* Max number of I/O plugins */
#define MAX_NUM_SVC      64	     /* Enough? */
#define MAX_NUM_SVC_ARGS 16

/* Default enable for all services, can be stopped by means
 * of issuing an initctl call. E.g.
 *   initctl <stop|start|restart> service */
typedef struct svc {
	/* Private */
	int            id;	/* Service ID#, set by svc_new() */
	pid_t	       pid;
	char	       cmd[MAX_ARG_LEN];
	char	       args[MAX_NUM_SVC_ARGS][MAX_ARG_LEN];
	char	       descr[MAX_STR_LEN];
	char           username[MAX_USER_LEN];
	/* Public */
	int	       delayed_reload; /* For external service plugin. */
	plugin_svc_t  *plugin;
} svc_t;

typedef struct svc_map svc_map_t;

/* Put services array in shm */
static inline svc_t *finit_svc_connect(void)
{
	const long ID = FINIT_SHM_ID;
	static void *ptr = (void *)-1;

	if ((void *)-1 == ptr) {
		ptr = shmat (shmget (ID, sizeof(svc_t) * MAX_NUM_SVC, 0600 | IPC_CREAT), NULL, 0);
		if ((void *)-1 == ptr) {
			_d("Failed allocating shared memory, error %d: %s", errno, strerror (errno));
			return NULL;
		}
	}

	return (svc_t *)ptr;
}

static inline int svc_id(svc_t *svc)
{
	if (!svc) {
		errno = EINVAL;
		return -1;
	}

	return svc->id;
}

svc_t    *svc_new           (void);
svc_t    *svc_find_by_id    (int id);
svc_t    *svc_find_by_name  (char *name);
svc_t    *svc_iterator      (int restart);

int       svc_register      (char *line, char *username);
int       svc_id_by_name    (char *name);
svc_cmd_t svc_enabled	    (svc_t *svc, int dynamic);
int       svc_start         (svc_t *svc);
int       svc_start_by_name (char *name);
int       svc_stop          (svc_t *svc);
int       svc_reload        (svc_t *svc);
void      svc_start_all     (void);
void      svc_monitor       (void);

#endif	/* FINIT_SVC_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
