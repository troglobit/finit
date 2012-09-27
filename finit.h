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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* Distribution configuration */

#if defined EMBEDDED_SYSTEM
# define CONSOLE        "/dev/console"
# define MDEV           "/sbin/mdev"
# define GETTY          "/sbin/getty -L 115200 ttyS0 vt100"
# define LISTEN_INITCTL
#else /* Debian/Ubuntu based distributions */
# define RANDOMSEED	"/var/lib/urandom/random-seed"
# define CONSOLE        "/dev/tty1"
# define GETTY		"/sbin/getty -8 38400 tty1"
# define REMOUNT_ROOTFS_RW
# define RUNLEVEL	2
# define LISTEN_INITCTL
# define USE_UDEV
# define USE_MESSAGE_BUS
#endif

#ifndef DEFUSER
# define DEFUSER "root"
#endif
#ifndef DEFHOST
# define DEFHOST "noname"
#endif

#define CMD_SIZE  256
#define LINE_SIZE 1024
#define BUF_SIZE  4096
#define USERNAME_SIZE 16
#define HOSTNAME_SIZE 32

extern char *sdown;
extern char *network;
extern char *hostname;
extern char *username;

/* conf.c */
void parse_finit_conf(char *file);

#endif /* FINIT_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
