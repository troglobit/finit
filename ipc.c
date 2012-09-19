/* Generic IPC class based on TIPC as message bus
 *
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

#include <errno.h>
#include <poll.h>
#include <string.h>
//#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/tipc.h>

#include "finit.h"
#include "ipc.h"

#define IPC_MAP_SOCKADDR(a,t,p)                 \
   {                                            \
      (a).family   = AF_TIPC;                   \
      (a).addrtype = TIPC_ADDR_MCAST;           \
      (a).scope    = TIPC_CLUSTER_SCOPE;        \
      (a).addr.nameseq.type = (t);              \
      (a).addr.nameseq.lower = (p);             \
      (a).addr.nameseq.upper = (p);             \
   }

/**
 * ipc_init - Create a TIPC socket for receiving or sending events.
 * @type: The msg bus.
 * @port: The msg port.
 * @conn_type: SERVER or CLIENT connection.
 *
 * Returns: 
 * The resulting socket on success, -1 on error.
 */
int ipc_init(int type, int port, int conn_type)
{
	int sd = -1;
	int restart_count = 0;

 restart:
	while (-1 == sd) {
		struct sockaddr_tipc tipc_addr;

		if (restart_count++ > 5) {
			_e("Failed too many times, aborting.");
			restart_count = 0;

			return -1;
		}

		sd = socket(AF_TIPC, SOCK_RDM, 0);
		if (-1 == sd) {
			_pe("Failed creating TIPC socket");
			sleep(1);
			goto restart;
		}

		/* In case the user wants a server socket. */
		IPC_MAP_SOCKADDR(tipc_addr, type, port);
		/* bind to the socket if it will work as a server part */
		if (conn_type) {
			if (bind
			    (sd, (struct sockaddr *)&tipc_addr,
			     sizeof(tipc_addr))) {
				_pe("TIPC Port {%u,%u,%u} could not be created",
                                       tipc_addr.addr.nameseq.type,
                                       tipc_addr.addr.nameseq.lower,
                                       tipc_addr.addr.nameseq.upper);
				close(sd);
				return -1;
			}
		}

		/* Clear counter */
		restart_count = 0;
	}

	/* OK, give caller bound socket */
	return sd;
}

/**
 * ipc_send_event - Send a TIPC event.
 * @sd: TIPC descriptor
 * @event: Data buffer to send.
 * @sz: Length of @event data.
 * @bus: ID of tipc bus
 *
 * Returns:
 * Posix OK(0) on OK or 1 on error.
 */
int ipc_send_event(int sd, void *event, size_t sz, int bus)
{
	int retries = 0;
	struct sockaddr_tipc tipc_addr;

	if (!event) {
		errno = EINVAL;
		return 1;
	}

	memset(&tipc_addr, 0, sizeof(tipc_addr));
	tipc_addr.family = AF_TIPC;
	tipc_addr.addrtype = TIPC_ADDR_MCAST;
	tipc_addr.addr.nameseq.type = SINGLE_TIPC_NODE;
	tipc_addr.addr.nameseq.lower = bus;
	tipc_addr.addr.nameseq.upper = bus;

	while (-1 == sendto(sd, (char *)event, sz, 0, (struct sockaddr *)&tipc_addr, (socklen_t) sizeof(tipc_addr))) {
		switch (errno) {
		case EINTR:
			/* Signal received, resume syscall. */
			continue;

		case ENOTSOCK:
		case EBADF:
			if (retries++ > 1) {
				_e("Too many errors while trying to send.");
				return 1;
			}
			continue;

		default:
			return 1;
		}
	}

	return 0;
}

/**
 * ipc_receive_event - Poll or wait for a TIPC event.
 * @sd: TIPC descriptor.
 * @event: Return buffer for the returned link event.
 * @sz: Size of return buffer.
 * @timeout: Set to 0 to block, or >0 to have a timeout in milliseconds.
 *
 * Returns:
 * Zero (0) on OK or 1 on error.
 */
int ipc_receive_event(int sd, void *event, size_t sz, int timeout)
{
	struct sockaddr_tipc tipc_addr;
	socklen_t alen = sizeof(struct sockaddr_tipc);

	if (!event) {
		errno = EINVAL;
		return 1;
	}

	/* The poll() syscall handles timeout==0 differently  */
	while (timeout > 0) {
		int ret = 0;
		struct pollfd fd = { sd, POLLIN | POLLPRI, 0 };

		ret = poll(&fd, 1, timeout);
		if (0 == ret) {
			/* Timeout */
			errno = ETIMEDOUT;

			return 1;
		}
		if (-1 == ret) {
			if (EINTR == errno) {
				/* Unlike the below call to recvfrom() we return here,
				 * because a signal received here could be intended.  This
				 * only affects those tasks calling us with a timeout > 0
				 * and they usually know about this, and exploit it.
				 * E.g. by sending kill() to the pthread in charge of link
				 * propagation within a process.  One such user is
				 * RSTP. --Jocke 2007-10-15 */
				_d("poll() received signal...");
				errno = EINTR;
				return 1;
			}

			_pe("poll() failed.");
			return 1;
		}

		/* Check returned events on our TIPC socket. */
		if (fd.revents & (POLLIN | POLLPRI)) {
			/* OK reply, activity on sd */
			break;
		}

		/* No input, only error. */
		return 1;
	}

	while (-1 == recvfrom(sd, event, sz, 0, (struct sockaddr *)&tipc_addr, &alen)) {
		switch (errno) {
		case EINTR:
			/* Signal received, resume syscall. */
			continue;

		default:
			_pe("Failed reading TIPC event");
			return 1;
		}
	}

	return 0;
}

/**
 * ipc_wait_event - "Robust" IPC receive, waits with timeout and signal resistance before giving up.
 * @sd: TIPC descriptor.
 * @event: Event type to wait for.
 * @timeout: Set to 0 to block, or >0 to have a timeout in seconds.
 *
 * Returns:
 * Zero (0) on OK or 1 on timeout.
 */
int ipc_wait_event(int sd, int event, int timeout)
{
	generic_event_t ev;
	struct timeval start, now, diff;

	/* Poll for activation acknowledgement */
	gettimeofday(&start, NULL);
	do {
		if (ipc_receive_event(sd, &ev, sizeof(ev), timeout * 1000)) {
			/* All syscalls may be interrupted by signal, in which case
			 * errno is set to EINTR */
			if (errno == EINTR) {
				_d("GOT SIGNAL");
				continue;
			}
			if (errno == ETIMEDOUT) {
				_d("timeout");
				return 1;
			}

			_d("Failed with error %d", errno);
		}

		_d("Received event 0x%lx, expected 0x%x", ev.mtype, event);

		gettimeofday(&now, NULL);
		timersub(&now, &start, &diff);
		if (diff.tv_sec >= timeout)
			return 1;
	} while (ev.mtype != event);

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
