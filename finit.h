/* Improved fast init
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

#ifndef FINIT_H_
#define FINIT_H_

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <syslog.h>
#define DO_LOG(level, fmt, args...)				\
{								\
	openlog("finit", LOG_CONS | LOG_PID, LOG_DAEMON);	\
	syslog(LOG_DEBUG, fmt, ##args);				\
	closelog();						\
}

#define DEBUG(fmt, args...)  DO_LOG(LOG_DEBUG, fmt, ##args)
#define ERROR(fmt, args...)  DO_LOG(LOG_CRIT, fmt, ##args)

#define FINIT_FIFO "/dev/initctl"
#define FINIT_CONF "/etc/finit.conf"
#define FINIT_RCSD "/etc/finit.d"

/* Distribution configuration */

#if defined EMBEDDED_SYSTEM
# define CONSOLE        "/dev/console"
# define MDEV           "/sbin/mdev"
# define GETTY          "/sbin/getty -L 115200 ttyS0 vt100"
# define LISTEN_INITCTL
#elif defined DIST_MDV	/* Mandriva */
# define RANDOMSEED	"/var/lib/random-seed"
# define SYSROOT	"/sysroot"
# define CONSOLE        "/dev/console"
# define GETTY		"/usr/bin/openvt /sbin/mingetty tty2"
# define RUNPARTS	"/usr/bin/run-parts"
# define REMOUNT_ROOTFS_RW
# define PAM_CONSOLE
# define LISTEN_INITCTL
# define RUNLEVEL	5
# define USE_MESSAGE_BUS
#elif defined DIST_EEEXUBUNTU	/* eeeXubuntu */
# define RANDOMSEED	"/var/lib/urandom/random-seed"
# define SYSROOT	"/sysroot"
# define CONSOLE        "/dev/console"
# define GETTY		"/usr/bin/openvt /sbin/getty 38400 tty2"
# define RUNPARTS	"/bin/run-parts"
# define REMOUNT_ROOTFS_RW
# define TOUCH_ETC_NETWORK_RUN_IFSTATE
# define LISTEN_INITCTL
#elif defined DIST_ORIGEEE	/* original EeePC distribution */
# define RANDOMSEED	"/var/lib/urandom/random-seed"
# define SYSROOT	"/mnt"
# define CONSOLE        "/dev/console"
# define GETTY		"/usr/bin/openvt /sbin/getty 38400 tty2"
# define RUNPARTS	"/bin/run-parts"
# define TOUCH_ETC_NETWORK_RUN_IFSTATE
# define USE_ETC_RESOLVCONF_RUN
#else                           /* Ubuntu */
# define RANDOMSEED	"/var/lib/urandom/random-seed"
# define CONSOLE        "/dev/tty1"
# define GETTY		"/sbin/getty -8 38400 tty1"
# define REMOUNT_ROOTFS_RW
# define RUNLEVEL	2
# define LISTEN_INITCTL
# define USE_UDEV
# define USE_MESSAGE_BUS
#endif

#define SYNC_SHUTDOWN   "/var/lock/finit.shutdown"
#define SYNC_STOPPED    "/var/lock/finit.stopped"

#ifndef DEFUSER
# define DEFUSER "root"
#endif
#ifndef DEFHOST
# define DEFHOST "noname"
#endif

#define LINE_SIZE 1024
#define CMD_SIZE 256
#define USERNAME_SIZE 16
#define HOSTNAME_SIZE 32

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

/* Match one command. */
#define MATCH_CMD(l, c, x)					\
	(!strncmp(l, c, strlen(c)) && (x = (l) + strlen(c)))

#define _d(fmt, args...) do { if (debug) { fprintf(stderr, "finit:%s() - " fmt "\n", __func__, ##args); } } while (0)

extern int   debug;
extern char *sdown;

/* signal.c */
void do_shutdown    (int sig);
void sig_init       (void);
void sig_setup      (void);

/* initctl.c */
void listen_initctl (void);

#endif /* FINIT_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
