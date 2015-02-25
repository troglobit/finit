/* Classic inetd services launcher for Finit
 *
 * Copyright (c) 2015  Joachim Nilsson <troglobit@gmail.com>
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

#ifndef FINIT_INETD_H_
#define FINIT_INETD_H_

#include <netdb.h>
#include <net/if.h>

#include "queue.h"
#include "libuev/uev.h"


typedef struct inetd_filter {
	LIST_ENTRY(inetd_filter) link;
	int  deny;		/* 0:allow, 1:deny */
	char ifname[IFNAMSIZ];	/* E.g., eth0 */
} inetd_filter_t;

typedef struct {
	uev_t  watcher;

	int    type;		/* Socket type: SOCK_STREAM/SOCK_DGRAM    */
	int    std;		/* Standard proto/port from /etc/services */
	int    proto;
	int    port;
	int    forking;
	char   name[10];
	int  (*cmd)(int type);	/* internal inetd service, like 'time' */

	LIST_HEAD(, inetd_filter) filters;
} inetd_t;

int  inetd_dgram_peek  (int sd, char *ifname);
int  inetd_stream_peek (int sd, char *ifname);

int  inetd_respawn (pid_t pid);
void inetd_runlevel(uev_ctx_t *ctx, int runlevel);

int  inetd_match (inetd_t *inetd, char *service, char *proto, char *port);
int  inetd_add   (inetd_t *inetd, char *service, char *proto, char *ifname, char *port, int forking);

inetd_filter_t *inetd_filter_find      (inetd_t *inetd, char *ifname);
int             inetd_allow_iface      (inetd_t *inetd, char *ifname);
int             inetd_is_iface_allowed (inetd_t *inetd, char *ifname);

#endif	/* FINIT_INETD_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
