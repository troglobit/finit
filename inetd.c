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

#include "inetd.h" /* XXX: Move back down after upgrading to libuEv >1.0.4 */

#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "libuev/uev.h"
#include "libite/lite.h"

#include "finit.h"
#include "helpers.h"
#include "private.h"
#include "svc.h"


/* Socket callback, looks up correct svc and starts it as an inetd service */
static void socket_cb(uev_ctx_t *UNUSED(ctx), uev_t *w, void *arg, int UNUSED(events))
{
	svc_t *svc = (svc_t *)arg;

	if (SVC_START != svc_enabled(svc, -1, NULL))
		return;

	if (!svc->inetd.forking)
		uev_io_stop(w);

	svc_start(svc);
}

/* Launch Inet socket for service.
 * TODO: Add filtering ALLOW/DENY per interface.
 */
static void spawn_socket(inetd_t *inetd)
{
	int sd;
	socklen_t len = sizeof(struct sockaddr);
	struct sockaddr_in s;

	if (!inetd->type) {
		FLOG_ERROR("Skipping invalid inetd service %s", inetd->name);
		return;
	}

	sd = socket(AF_INET, inetd->type | SOCK_NONBLOCK, inetd->proto);
	if (-1 == sd) {
		FLOG_PERROR("Failed opening inetd socket type %d proto %d", inetd->type, inetd->proto);
		return;
	}

	memset(&s, 0, sizeof(s));
	s.sin_family      = AF_INET;
	s.sin_addr.s_addr = INADDR_ANY;
	s.sin_port        = htons(inetd->port);
	if (bind(sd, (struct sockaddr *)&s, len) < 0) {
		FLOG_PERROR("Failed binding to port %d, maybe another %s server is already running",
			    inetd->port, inetd->name);
		close(sd);
		return;
	}

	if (inetd->port) {
		if (inetd->type == SOCK_STREAM) {
			if (-1 == listen(sd, 20)) {
				FLOG_PERROR("Failed listening to inetd service %s", inetd->name);
				close(sd);
				return;
			}
		} else {           /* SOCK_DGRAM */
			int opt = 1;

			/* Set extra sockopt to get ifindex from inbound packets */
			if (-1 == setsockopt(sd, SOL_IP, IP_PKTINFO, &opt, sizeof(opt)))
				_pe("Failed enabling IP_PKTINFO on socket");
		}
	}

	uev_io_init(ctx, &inetd->watcher, socket_cb, inetd->arg, sd, UEV_READ);
}

/* Peek into SOCK_DGRAM socket to figure out where an inbound packet comes from. */
int inetd_dgram_peek(int sd, char *ifname)
{
	struct msghdr msgh;
	struct cmsghdr *cmsg;

	memset(&msgh, 0, sizeof(msgh));
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
	svc_t *svc = svc_find_by_pid(pid);

	if (svc && SVC_CMD_INETD == svc->type) {
		inetd_t *inetd = &svc->inetd;

		svc->pid = 0;
		if (!inetd->forking)
			uev_io_set(&inetd->watcher, inetd->watcher.fd, UEV_READ);

		return 1;	/* It was us! */
	}

	return 0;		/* Not an inetd service */
}


void inetd_start(inetd_t *inetd)
{
	if (inetd->watcher.fd == -1)
		spawn_socket(inetd);
}

void inetd_stop(inetd_t *inetd)
{
	svc_t *svc = (svc_t *)inetd->arg;

	if (inetd->watcher.fd != -1) {
		uev_io_stop(&inetd->watcher);
		close(inetd->watcher.fd);
		inetd->watcher.fd = -1;
	}

	/* Stop any running service, not allowed anymore. */
	if (svc->pid)
		svc_stop(svc);
}

static int getent(char *service, char *proto, struct servent **sv, struct protoent **pv)
{
	*sv = getservbyname(service, proto);
	if (!*sv) {
		_pe("Invalid inetd %s/%s (service/proto), skipping", service, proto);
		return errno = EINVAL;
	}

	if (pv) {
		*pv = getprotobyname((*sv)->s_proto);
		if (!*pv) {
			_pe("Cannot find proto %s, skipping.", (*sv)->s_proto);
			return errno = EINVAL;
		}
	}

	return 0;
}

/*
 * Find exact match.
 */
static inetd_filter_t *find_filter(inetd_t *inetd, char *ifname)
{
	inetd_filter_t *filter;

	if (!ifname)
		ifname = "";

	LIST_FOREACH(filter, &inetd->filters, link) {
		_d("Checking filters for %s: '%s' vs '%s' (exact match) ...",
		   inetd->name, filter->ifname, ifname);

		if (!strcmp(filter->ifname, ifname))
			return filter;
	}

	return NULL;
}

/*
 * First try exact match, then fall back to any match.
 */
inetd_filter_t *inetd_filter_match(inetd_t *inetd, char *ifname)
{
	inetd_filter_t *filter = find_filter(inetd, ifname);

	if (filter)
		return filter;

	if (!ifname)
		ifname = "";

	LIST_FOREACH(filter, &inetd->filters, link) {
		_d("Checking filters for %s: '%s' vs '%s' (any match) ...",
		   inetd->name, filter->ifname, ifname);

		if (!strlen(filter->ifname))
			return filter;
	}

	return NULL;
}

int inetd_allow(inetd_t *inetd, char *ifname)
{
	inetd_filter_t *filter;

	if (!inetd)
		return errno = EINVAL;

	filter = inetd_filter_match(inetd, ifname);
	if (filter) {
		_d("Filter %s for inetd %s already exists, skipping.", ifname ?: "*", inetd->name);
		return 0;
	}

	_d("Allow iface %s for service %s (port %d)", ifname ?: "*", inetd->name, inetd->port);
	filter = calloc(1, sizeof(*filter));
	if (!filter) {
		_e("Out of memory, cannot add filter to service %s", inetd->name);
		return errno = ENOMEM;
	}

	if (!ifname)
		ifname = "";
	filter->deny = 0;
	strlcpy(filter->ifname, ifname, sizeof(filter->ifname));
	LIST_INSERT_HEAD(&inetd->filters, filter, link);

	return 0;
}

int inetd_deny(inetd_t *inetd, char *ifname)
{
	inetd_filter_t *filter;

	if (!inetd)
		return errno = EINVAL;

	/* Reset to NULL for debug output below */
	if (!ifname[0])
		ifname = NULL;

	filter = find_filter(inetd, ifname);
	if (filter) {
		_d("%s filter %s for inetd %s already exists, cannot set deny filter for same, skipping.",
		   filter->deny ? "Deny" : "Allow", ifname ?: "*", inetd->name);
		return 1;
	}

	_d("Deny iface %s for service %s (port %d)", ifname ?: "*", inetd->name, inetd->port);
	filter = calloc(1, sizeof(*filter));
	if (!filter) {
		_e("Out of memory, cannot add filter to service %s", inetd->name);
		return errno = ENOMEM;
	}

	if (!ifname)
		ifname = "";
	filter->deny = 1;
	strlcpy(filter->ifname, ifname, sizeof(filter->ifname));
	LIST_INSERT_HEAD(&inetd->filters, filter, link);

	return 0;
}

int inetd_is_allowed(inetd_t *inetd, char *ifname)
{
	inetd_filter_t *filter;

	if (!inetd) {
		errno = EINVAL;
		return 0;
	}

	filter = inetd_filter_match(inetd, ifname);
	if (filter) {
		_d("Found matching filter for %s, deny: %d ... ", inetd->name, filter->deny);
		return !filter->deny;
	}

	_d("No matching filter for %s ... ", inetd->name);

	return 0;
}

int inetd_match(inetd_t *inetd, char *service, char *proto, char *port)
{
	int cport = port ? atonum(port) : -1;

	if (!inetd || !service || !proto)
		return errno = EINVAL;

	if (strncmp(inetd->name, service, sizeof(inetd->name)))
		return 0;

	if (cport != -1) {
		if (inetd->port == cport)
			return 1;
	} else {
		struct servent *sv = NULL;

		if (getent(service, proto, &sv, NULL))
			return 0;

		if (inetd->port == ntohs(sv->s_port))
			return 1;
	}

	return 0;
}

/*
 * This function is called to add a new, unique, inetd service.  When an
 * ifname is given as argument this means *only* run service on this
 * interface.  If a similar service runs on another port, this function
 * must add this ifname as "deny" to that other service.
 *
 * Example:
 *     inetd ssh@eth0:222/tcp nowait [2345] /usr/sbin/sshd -i
 *     inetd ssh/tcp          nowait [2345] /usr/sbin/sshd -i
 *
 * In this example eth0:222 is very specific, so when ssh/tcp (default)
 * is added we must find the previous 'ssh' service and add its eth0 as
 * deny, so we don't accept port 22 session on eth0.
 *
 * In the reverse case, where the default (ssh/tcp) entry is listed
 * before the specific (eth0:222), the new inetd service must find the
 * (any!) previous rule and add its ifname to their deny list.
 *
 * If equivalent service exists already svc_register() will instead call
 * inetd_allow().
 */
int inetd_new(inetd_t *inetd, char *service, char *proto, int forking)
{
	int result;
	struct servent  *sv = NULL;
	struct protoent *pv = NULL;

	if (!inetd || !service || !proto)
		return errno = EINVAL;

	result = getent(service, proto, &sv, &pv);
	if (result)
		return result;

	/* Setup defaults */
	inetd->std     = 1;
	inetd->port    = ntohs(sv->s_port);
	inetd->proto   = pv->p_proto;
	inetd->forking = !!forking;
	strlcpy(inetd->name, service, sizeof(inetd->name));

	/* Naïve mapping tcp->stream, udp->dgram, other->dgram */
	if (!strcasecmp(sv->s_proto, "tcp"))
		inetd->type = SOCK_STREAM;
	else
		inetd->type = SOCK_DGRAM;

	/* Reset descriptor, used internally */
	inetd->watcher.fd = -1;

	_d("New service %s (default port %d proto %s:%d)", service, inetd->port, sv->s_proto, pv->p_proto);

	return 0;
}

int inetd_del(inetd_t *inetd)
{
	inetd_filter_t *filter, *tmp;

	LIST_FOREACH_SAFE(filter, &inetd->filters, link, tmp)
		free(filter);

	return 0;
}

int inetd_init(inetd_t *inetd, void *arg, char *ifname, char *port)
{
	int result, cport = -1;
	svc_t *svc;

	if (!inetd || !arg)
		return errno = EINVAL;

	if (port)
		cport = atonum(port);

	if (cport != -1 && cport != inetd->port) {
		inetd->std  = 0;
		inetd->port = cport;
	}

	/* Setup socket callback argument */
	inetd->arg = arg;

	/* Poor man's tcpwrappers filtering */
	result = inetd_allow(inetd, ifname);

	/* For each similar service, on other port, add their ifnames as deny to ours. */
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		inetd_filter_t *filter;

		/* Skip non-inetd services */
		if (svc->type != SVC_CMD_INETD)
			continue;

		/* Skip ourselves */
		if (&svc->inetd == inetd)
			continue;

		/* Skip different service types (telnet != ssh) */
		if (strcmp(svc->inetd.name, inetd->name))
			continue;

		/* Deny all their interfaces from using my service */
		LIST_FOREACH(filter, &svc->inetd.filters, link) {
			if (filter->deny)
				continue;

			inetd_deny(inetd, filter->ifname);
		}

		/* Deny my interfaces from using their service ... */
		LIST_FOREACH(filter, &inetd->filters, link) {
			if (filter->deny)
				continue;

			inetd_deny(&svc->inetd, filter->ifname);
		}
	}

	return result;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
