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

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "finit.h"
#include "svc.h"
#include "helpers.h"
#include "libuev/uev.h"

/* Socket callback, looks up correct svc and starts it as an inetd service */
static void socket_cb(uev_ctx_t *UNUSED(ctx), uev_t *w, void *arg, int UNUSED(events))
{
	svc_t *svc = (svc_t *)arg;

	if (SVC_START != svc_enabled(svc, 1, NULL))
		return;

	if (!svc->forking)
		uev_io_stop(w);

	svc_start(svc);
}

/* Launch Inet socket for service.
 * TODO: Add filtering ALLOW/DENY per interface.
 */
static void spawn_socket(uev_ctx_t *ctx, svc_t *svc)
{
	int sd;
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in s;

	if (!svc->sock_type) {
		FLOG_ERROR("Skipping invalid inetd service %s", svc->cmd);
		return;
	}

	sd = socket(AF_INET, svc->sock_type | SOCK_NONBLOCK, svc->proto);
	if (-1 == sd) {
		FLOG_PERROR("Failed opening inetd socket type %d proto %d", svc->sock_type, svc->proto);
		return;
	}

	memset(&s, 0, sizeof(s));
	s.sin_family      = AF_INET;
	s.sin_addr.s_addr = INADDR_ANY;
	s.sin_port        = htons(svc->port);
	if (bind(sd, (struct sockaddr *)&s, len) < 0) {
		FLOG_PERROR("Failed binding to port %d, maybe another %s server is already running",
			    svc->port, svc->service);
		close(sd);
		return;
	}

	if (svc->port) {
		if (svc->sock_type == SOCK_STREAM) {
			if (-1 == listen(sd, 20)) {
				FLOG_PERROR("Failed listening to inetd service %s", svc->service);
				close(sd);
				return;
			}
		} else {           /* SOCK_DGRAM */
			int opt = 1;

			/* Set extra sockopt to get ifindex from inbound packets */
			setsockopt(sd, SOL_IP, IP_PKTINFO, &opt, sizeof(opt));
		}
	}

	_d("Initializing inetd %s service %s type %d proto %d on socket %d ...",
	   svc->service, basename(svc->cmd), svc->sock_type, svc->proto, sd);
	uev_io_init(ctx, &svc->watcher, socket_cb, svc, sd, UEV_READ);
}

/* Peek into SOCK_DGRAM socket to figure out where an inbound packet comes from. */
int inetd_dgram_peek(int sd, char *ifname)
{
	struct msghdr msgh;
	struct cmsghdr *cmsg;

	if (recvmsg(sd, &msgh, MSG_PEEK) < 0)
		return -1;

	for (cmsg = CMSG_FIRSTHDR(&msgh); cmsg; cmsg = CMSG_NXTHDR(&msgh,cmsg)) {
		struct in_pktinfo *ipi = (struct in_pktinfo *)CMSG_DATA(cmsg);

		if (cmsg->cmsg_level != SOL_IP || cmsg->cmsg_type != IP_PKTINFO)
			continue;

		if_indextoname(ipi->ipi_ifindex, ifname);

		return 0;
	}

	return -1;
}

/* Peek into SOCK_STREAM on accepted client socket to figure out inbound interface */
int inetd_stream_peek(int sd, char *ifname)
{
	struct ifaddrs *ifaddr, *ifa;
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	if (-1 == getsockname(sd, (struct sockaddr *)&sin, &len))
		return -1;

	if (-1 == getifaddrs(&ifaddr))
		return -1;

	for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
		size_t len = sizeof(struct in_addr);
		struct sockaddr_in *iin;

		if (!ifa->ifa_addr)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		iin = (struct sockaddr_in *)ifa->ifa_addr;
		if (!memcmp(&sin.sin_addr, &iin->sin_addr, len)) {
			strncpy(ifname, ifa->ifa_name, IF_NAMESIZE);
			break;
		}
	}

	freeifaddrs(ifaddr);

	return 0;
}

/* Inetd monitor, called by svc_monitor() */
int inetd_respawn(pid_t pid)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (SVC_CMD_INETD != svc->type)
			continue;

		if (svc->pid == pid) {
			svc->pid = 0;
			if (!svc->forking)
				uev_io_set(&svc->watcher, svc->watcher.fd, UEV_READ);

			return 1; /* It was us! */
		}
	}

	return 0;		  /* Not an inetd service */
}

/* Called when changing runlevel to start Inet socket handlers */
void inetd_runlevel(uev_ctx_t *ctx, int runlevel)
{
	svc_t *svc;

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (SVC_CMD_INETD != svc->type)
			continue;

		if (!ISSET(svc->runlevels, runlevel))
			continue;

		
		spawn_socket(ctx, svc);
	}
}


/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
