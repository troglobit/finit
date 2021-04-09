/* Misc. utility functions and C-library extensions for finit and its plugins
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

#ifndef FINIT_HELPERS_H_
#define FINIT_HELPERS_H_

#include <mntent.h>
#include <sched.h>		/* sched_yield() */
#include <stdarg.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/ttydefaults.h>	/* Not included by default in musl libc */
#include <termios.h>
#include <lite/lite.h>
#include "log.h"

#define PROGRESS_DEFAULT PROGRESS_MODERN

typedef enum {
	PROGRESS_SILENT,
	PROGRESS_CLASSIC,
	PROGRESS_MODERN,
} pstyle_t;

#ifndef HAVE_GETFSENT
struct fstab {
	char       *fs_spec;       /* block device name */
	char       *fs_file;       /* mount point */
	char       *fs_vfstype;    /* file-system type */
	char       *fs_mntops;     /* mount options */
	const char *fs_type;       /* rw/rq/ro/sw/xx option */
	int         fs_freq;       /* dump frequency, in days */
	int         fs_passno;     /* pass number on parallel dump */
};

int           setfsent  (void);
void          endfsent  (void);
struct fstab *getfsent  (void);
#endif	/* HAVE_GETFSENT */

char   *console         (void);
void    console_init    (void);
ssize_t cprintf         (const char *fmt, ...);

char   *strip_line      (char *line);

void    enable_progress (int onoff);
void    show_progress   (pstyle_t style);

int     getty           (char *tty, speed_t speed, char *term, char *user);
int     sh              (char *tty);

void    stty            (int fd, speed_t speed);
speed_t stty_parse_speed(char *baud);

void    print_banner    (const char *heading);
void    printv          (const char *fmt, va_list ap);
void    print           (int action, const char *fmt, ...);
void    print_desc      (char *action, char *desc);
int     print_result    (int fail);

int     getuser         (char *username, char **home);
int     getgroup        (char *group);

int     getcuser        (char *buf, size_t len);
int     getcgroup       (char *buf, size_t len);

int     mksubsys        (const char *dir, mode_t mode, char *user, char *group);

void    set_hostname    (char **hostname);
void    networking      (int updown);

int     complete        (char *cmd, int pid);
int     run             (char *cmd);
int     run_interactive (char *cmd, char *fmt, ...);
int     exec_runtask    (char *cmd, char *args[]);
pid_t   run_getty       (char *tty, char *baud, char *term,  int noclear, int nowait, struct rlimit rlimit[]);
pid_t   run_getty2      (char *tty, char *cmd, char *args[], int noclear, int nowait, struct rlimit rlimit[]);
pid_t   run_sh          (char *tty, int noclear, int nowait, struct rlimit rlimit[]);
int     run_parts       (char *dir, char *cmd);

static inline int create(char *path, mode_t mode, uid_t uid, gid_t gid)
{
	if (touch(path) || chmod(path, mode) || chown(path, uid, gid)) {
		_w("Failed creating %s properly.", path);
		return -1;
	}

	return 0;
}

static inline int dprint(int fd, const char *s, size_t len)
{
	size_t loop = 3;
	int rc = -1;

	if (!len)
		len = strlen(s);

	while (loop--) {
		rc = write(fd, s, len);
		if (rc == -1 && errno == EINTR)
			continue;
		break;
	}

	return rc;
}

int	ismnt		(char *file, char *dir, char *mode);
int	fismnt		(char *dir);

#endif /* FINIT_HELPERS_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
