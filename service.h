/* Service starter and supervisor, inspired by daemontools
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

#ifndef FINIT_SERVICES_H_
#define FINIT_SERVICES_H_

#include <errno.h>
#include <sys/shm.h>		/* shmat() */
#include <sys/types.h>		/* pid_t */

#include "finit.h"

#define FINIT_SHM_ID     0x494E4954  /* "INIT", see ascii(7) */
#define MAX_NUM_SVC      64	     /* Enough? */
#define MAX_NUM_SVC_ARGS 16

/* Default enable for all services, can be stopped by means
 * of issuing an initctl call. E.g.
 *   initctl service <stop|start|restart> */
typedef struct svc {
	char	       cmd[64];
	char	       args[MAX_NUM_SVC_ARGS][64];
	char	       descr[64];
	pid_t	       pid;
	int	       reload;
	int	       private;	/* For svc callbacks to use freely, possibly to store "states". */
	/* Set for known services, otherwise "empty". */
	struct svc_map {
		char *path;		      /* E.g., "/sbin/snmpd" */
		int (*cb)(struct svc *, int); /* Service callback, e.g. is_snmpd_enabled() */
		int   dyn_reload;	      /* Reload on new dynamic event. */
		int   dyn_workaround;	      /* Ugly/Temporary workaround for dyn-stop/start of serialoverip. */
		int   vlans_changed_reload;   /* Reload when VLANs change. */
	} reg;
} svc_t;

typedef struct svc_map svc_map_t;

/* Put services array in shm */
static inline svc_t *finit_connect_shm(void)
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

int     service_register      (char *line);
int     service_start	      (svc_t *svc);
int     service_start_by_name (char *name);
int     service_enabled	      (svc_t *svc, int dynamic);
void    service_startup	      (void);
void    service_monitor	      (void);

#endif	/* FINIT_SERVICES_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
