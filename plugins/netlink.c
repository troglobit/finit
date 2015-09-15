/* Netlink plugin for IFUP/IFDN and GW events
 *
 * Copyright (C) 2009-2011  Mårten Wikström <marten.wikstrom@keystream.se>
 * Copyright (C) 2009-2015  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <net/if.h>		/* IFNAMSIZ */
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <unistd.h>

#include "../finit.h"
#include "../event.h"
#include "../helpers.h"
#include "../plugin.h"

static int nlmsg_validate(struct nlmsghdr *nlmsg, size_t len)
{
	if (!NLMSG_OK(nlmsg, len))
		return 1;

	if (len < sizeof(struct nlmsghdr))
		return 1;

	if (len < nlmsg->nlmsg_len)
		return 1;

	return 0;
}

static void nl_route(struct nlmsghdr *nlmsg)
{
	struct rtmsg *r;
	struct rtattr *a;
	int la;
	int gw = 0, dst = 0, mask = 0, idx = 0;

	if (nlmsg->nlmsg_len < NLMSG_LENGTH(sizeof(struct rtmsg))) {
		_e("Packet too small or truncated!");
		return;
	}

	r  = NLMSG_DATA(nlmsg);
	a  = RTM_RTA(r);
	la = RTM_PAYLOAD(nlmsg);
	while (RTA_OK(a, la)) {
		void *data = RTA_DATA(a);
		switch (a->rta_type) {
		case RTA_GATEWAY:
			gw = *((int *)data);
			//_d("GW: 0x%04x", gw);
			break;

		case RTA_DST:
			dst = *((int *)data);
			mask = r->rtm_dst_len;
			//_d("MASK: 0x%04x", mask);
			break;

		case RTA_OIF:
			idx = *((int *)data);
			//_d("IDX: 0x%04x", idx);
			break;
		}

		a = RTA_NEXT(a, la);
	}

	if ((!dst && !mask) && (gw || idx)) {
		char msg[MAX_ARG_LEN];

		if (nlmsg->nlmsg_type == RTM_DELROUTE)
			snprintf(msg, sizeof(msg), "GW:DN");
		else
			snprintf(msg, sizeof(msg), "GW:UP");
		event_dispatch(msg);
	}
}

static void nl_link(struct nlmsghdr *nlmsg)
{
	int la;
	char ifname[IFNAMSIZ + 1];
	struct rtattr *a;
	struct ifinfomsg *i;

	if (nlmsg->nlmsg_len < NLMSG_LENGTH(sizeof(struct ifinfomsg))) {
		_e("Packet too small or truncated!");
		return;
	}

	i  = NLMSG_DATA(nlmsg);
	a  = (struct rtattr *)((char *)i + NLMSG_ALIGN(sizeof(struct ifinfomsg)));
	la = NLMSG_PAYLOAD(nlmsg, sizeof(struct ifinfomsg));

	while (RTA_OK(a, la)) {
		if (a->rta_type == IFLA_IFNAME) {
			char msg[MAX_ARG_LEN];

			strncpy(ifname, RTA_DATA(a), sizeof(ifname));
			switch (nlmsg->nlmsg_type) {
			case RTM_NEWLINK:
				/*
				 * New interface has appearad, or interface flags has changed.
				 * Check ifi_flags here to see if the interface is UP/DOWN
				 */
				if (i->ifi_change & IFF_UP)
					snprintf(msg, sizeof(msg), "IF%s:%s",
						 (i->ifi_flags & IFF_UP) ? "UP" : "DN",
						 ifname);
				else
					snprintf(msg, sizeof(msg), "IFADD:%s", ifname);
				event_dispatch(msg);
				break;

			case RTM_DELLINK:
				/* NOTE: Interface has dissapeared, not link down ... */
				snprintf(msg, sizeof(msg), "IFDEL:%s", ifname);
				event_dispatch(msg);
				break;

			case RTM_NEWADDR:
				_d("%s: New Address", ifname);
				break;

			case RTM_DELADDR:
				_d("%s: Deconfig Address", ifname);
				break;

			default:
				_d("%s: Msg 0x%x", ifname, nlmsg->nlmsg_type);
				break;
			}
		}

		a = RTA_NEXT(a, la);
	}
}

static void nl_callback(void *UNUSED(arg), int sd, int UNUSED(events))
{
	ssize_t len;
	static char replybuf[2048];
	struct nlmsghdr *p = (struct nlmsghdr *)replybuf;

	memset(replybuf, 0, sizeof(replybuf));
	len = recv(sd, replybuf, sizeof(replybuf), 0);
	if (len < 0) {
		if (errno != EINTR)	/* Signal */
			_pe("recv()");
		return;
	}

	for (; len > 0; p = NLMSG_NEXT(p, len)) {
		if (nlmsg_validate(p, len)) {
			_e("Packet too small or truncated!");
			return;
		}
		_d("Well formed netlink message received.");

		if (p->nlmsg_type == RTM_NEWROUTE || p->nlmsg_type == RTM_DELROUTE)
			nl_route(p);
		else
			nl_link(p);
	}
}

static plugin_t plugin = {
	.io = {
		.cb    = nl_callback,
		.flags = PLUGIN_IO_READ,
	},
};

PLUGIN_INIT(plugin_init)
{
	int sd;
	struct sockaddr_nl addr;

	sd = socket(PF_NETLINK, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_ROUTE);
	if (sd < 0) {
		_pe("socket()");
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_LINK;
	addr.nl_pid    = getpid();

	if (bind(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		_pe("bind()");
		close(sd);
		return;
	}

	plugin.io.fd = sd;
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
