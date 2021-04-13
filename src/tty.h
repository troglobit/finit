/* Finit TTY handling
 *
 * Copyright (c) 2013       Mattias Walström <lazzer@gmail.com>
 * Copyright (c) 2013-2021  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_TTY_H_
#define FINIT_TTY_H_

#include <stdio.h>
#include "svc.h"

struct tty {
	char	*cmd;
	char	*args[MAX_NUM_SVC_ARGS];
	size_t	 num;

	char	*dev;
	char	*baud;
	char	*term;
	int	 noclear;
	int	 nowait;
	int	 nologin;
};

char	*tty_canonicalize (char *dev);

int	 tty_isatcon      (char *dev);
int	 tty_atcon        (char *buf, size_t len);

int	 tty_parse_args   (char *cmdline, struct tty *tty);

int	 tty_exec	  (svc_t *tty);

#endif /* FINIT_TTY_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
