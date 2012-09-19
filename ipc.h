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

#ifndef FINIT_IPC_H_
#define FINIT_IPC_H_

#define SINGLE_TIPC_NODE         64
#define PRIMARY_MESSAGE_BUS      100
#define HWSETUP_MESSAGE_BUS      101
#define FINIT_MESSAGE_BUS        102

#define SERVER_CONNECTION 1
#define CLIENT_CONNECTION 0

/**
 * generic_event_t - Generic message type.
 * @mtype: Message type.
 *
 * This is the base message type that is used on the system primary message bus.  All
 * messages must start with a long @mtype that describes the type, any extended
 * messages must add its extra fields after @mtype.
 */
typedef struct {
   long mtype;
   long arg;                    /* Optional argument, free use. */
} generic_event_t;

int     ipc_init          (int type, int port, int conn_type);
int     ipc_receive_event (int sd, void *event, size_t sz, int timeout);
int     ipc_wait_event    (int sd, int event, int timeout);

#endif /* FINIT_IPC_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
