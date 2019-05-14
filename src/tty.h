/* Finit TTY handling
 *
 * Copyright (c) 2013       Mattias Walström <lazzer@gmail.com>
 * Copyright (c) 2013-2017  Joachim Nilsson <troglobit@gmail.com>
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

#include <limits.h>
#include <sys/resource.h>
#include <lite/queue.h>		/* BSD sys/queue.h API */

#define TTY_MAX_ARGS 16
#define EVENT_SIZE ((sizeof(struct inotify_event) + NAME_MAX + 1))

struct tty {
	LIST_ENTRY(tty) link;

	char   name[42];
	char   baud[10];
	char   term[10];
	int    noclear;
	int    nowait;
	int    nologin;
	int    runlevels;

	char  *cmd;		/* NULL when running built-in getty */
	char  *args[TTY_MAX_ARGS];

	int    pid;

	/* Limits and scoping */
	struct rlimit rlimit[RLIMIT_NLIMITS];

	/* Control group */
	char   cgroup[32];

	/* Set if modified => reloaded, or -1 when marked for removal */
	int    dirty;
};

void	    tty_mark	    (void);
void	    tty_sweep	    (void);

int	    tty_register    (char *line, struct rlimit rlimit[], char *cgroup, char *file);
int	    tty_unregister  (struct tty *tty);

struct tty *tty_find	    (char *dev);
size_t	    tty_num	    (void);
size_t      tty_num_active  (void);
struct tty *tty_find_by_pid (pid_t pid);
void	    tty_start	    (struct tty *tty);
void	    tty_stop	    (struct tty *tty);
int	    tty_enabled	    (struct tty *tty);
int	    tty_respawn	    (pid_t pid);
void	    tty_reload      (char *dev);
void	    tty_runlevel    (void);

#endif /* FINIT_TTY_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
