/* Signal management - Be conservative with what finit responds to!
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

#ifndef FINIT_SIG_H_
#define FINIT_SIG_H_

#include <signal.h>
#include <uev/uev.h>

#define SYNC_SHUTDOWN   "/var/run/finit/.shutdown"
#define SYNC_STOPPED    "/var/run/finit/.stopped"

#define SETSIG(sa, sig, fun, flags)			\
	do {						\
		sa.sa_sigaction = fun;			\
		sa.sa_flags = SA_SIGINFO | flags;	\
		sigemptyset(&sa.sa_mask);		\
		sigaction(sig, &sa, NULL);		\
	} while (0)

#define IGNSIG(sa, sig, flags)			\
	do {					\
		sa.sa_handler = SIG_IGN;	\
		sa.sa_flags = flags;		\
		sigemptyset(&sa.sa_mask);	\
		sigaction(sig, &sa, NULL);	\
	} while (0)

#define DFLSIG(sa, sig, flags)                  \
        do {                                    \
                sa.sa_handler = SIG_DFL;        \
                sa.sa_flags = flags;            \
                sigemptyset(&sa.sa_mask);       \
                sigaction(sig, &sa, NULL);      \
        } while (0)

/*
 * For old-style /dev/initctl shutdown we default to halt
 */
#define SHUT_DEFAULT SHUT_HALT

typedef enum {
	SHUT_OFF,
	SHUT_HALT,
	SHUT_REBOOT
} shutop_t;

extern shutop_t halt;

void do_shutdown    (shutop_t op);
int  sig_stopped    (void);
int  sig_num        (const char *name);
void sig_init       (void);
void sig_unblock    (void);
void sig_setup      (uev_ctx_t *ctx);

const char *sig_name(int signo);

#endif /* FINIT_SIG_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
