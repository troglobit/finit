/* Misc. utility functions and C-library extensions for finit and its plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
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

#ifndef FINIT_HELPERS_H_
#define FINIT_HELPERS_H_

#include <stdio.h>
#include <syslog.h>

#define DO_LOG(level, fmt, args...)				\
{								\
	openlog("finit", LOG_CONS | LOG_PID, LOG_DAEMON);	\
	syslog(LOG_DEBUG, fmt, ##args);				\
	closelog();						\
}

#define DEBUG(fmt, args...)  DO_LOG(LOG_DEBUG, fmt, ##args)
#define ERROR(fmt, args...)  DO_LOG(LOG_CRIT, fmt, ##args)

#ifndef touch
# define touch(x) mknod((x), S_IFREG|0644, 0)
#endif
#ifndef chardev
# define chardev(x,m,maj,min) mknod((x), S_IFCHR|(m), makedev((maj),(min)))
#endif
#ifndef blkdev
# define blkdev(x,m,maj,min) mknod((x), S_IFBLK|(m), makedev((maj),(min)))
#endif
#ifndef fexist
# define fexist(x) (access(x, F_OK) != -1)
#endif

#ifndef UNUSED
#define UNUSED(x) UNUSED_ ## x __attribute__ ((unused))
#endif

#define echo(fmt, args...) do { if (1) { fprintf(stderr, fmt "\n",  ##args); } } while (0)
#define _d(fmt, args...)   do { if (debug)   { fprintf(stderr, "finit:%s() - " fmt "\n", __func__, ##args); } } while (0)
#define _e(fmt, args...)   do { fprintf(stderr, "finit:%s() - " fmt "\n", __func__, ##args); } while (0)
#define _pe(fmt, args...)  do { fprintf(stderr, "finit:%s() - " fmt ". Error %d: %s\n", __func__, ##args, errno, strerror(errno)); } while (0)

extern int   debug;
extern int   verbose;

int	makepath	(char *path);
void    ifconfig        (char *ifname, char *addr, char *mask, int up);
void	copyfile	(char *src, char *dst, int size);

pid_t   pidfile_read    (const char *pidfile);
pid_t   pidfile_poll    (char *cmd, const char *path);

int     pid_alive       (pid_t pid);
char   *pid_get_name    (pid_t pid, char *name, size_t len);

void    procname_set    (const char *name, char *args[]);
int     procname_kill   (const char *name, int signo);

void    print_descr     (char *action, char *descr);
int     print_result    (int fail);
int     start_process   (char *cmd, char *args[], int console);
void    cls             (void);
void	chomp		(char *str);
int     getuser         (char *username);
int	getgroup	(char *group);
void    set_hostname    (char *hostname);

int     run             (char *cmd);
int     run_interactive (char *cmd, char *fmt, ...);
pid_t   run_getty       (char *cmd, char *argv[]);
int	run_parts	(char *dir, ...);

/* strlcpy.c */
size_t strlcpy(char *dst, const char *src, size_t siz);

#endif /* FINIT_HELPERS_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
