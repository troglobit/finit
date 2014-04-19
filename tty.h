/* Finit TTY handling
 *
 * Copyright (c) 2013  Mattias Walström <lazzer@gmail.com>
 * Copyright (c) 2013  Joachim Nilsson <troglobit@gmail.com>
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

#include "queue.h"		/* BSD sys/queue.h API */

#define EVENT_SIZE ((sizeof(struct inotify_event) + 20))

typedef struct {
	char  *name;
	int    baud;
	char  *term;
	int    runlevels;

	int    pid;
} finit_tty_t;

typedef struct tty_node {
	LIST_ENTRY(tty_node) link;
	finit_tty_t data;
} tty_node_t;

//extern LIST_HEAD(, tty_node) tty_list;

int	    tty_register    (char *line);
tty_node_t *tty_find	    (char *dev);
size_t	    tty_num	    (void);
tty_node_t *tty_find_by_pid (pid_t pid);
void	    tty_start	    (finit_tty_t *tty);
void	    tty_stop	    (finit_tty_t *tty);
int	    tty_enabled	    (finit_tty_t *tty, int runlevel);
int	    tty_respawn	    (pid_t pid);
void	    tty_runlevel    (int runlevel);

#endif /* FINIT_TTY_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
