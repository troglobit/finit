/* Finit - Fast /sbin/init replacement w/ I/O, hook & service plugins
 *
 * Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
 * Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lite/lite.h>
#include <uev/uev.h>

/* just in case */
#ifndef _PATH_STDPATH
#define _PATH_STDPATH   "/usr/bin:/bin:/usr/sbin:/sbin"
#endif

#ifndef _PATH_VARRUN
#define _PATH_VARRUN    "/var/run/"
#endif


#define CMD_SIZE                256
#define LINE_SIZE               1024
#define BUF_SIZE                4096

/*
 * We extend the INIT_CMD_ range for the new initctl tool, Finit only
 * support changing runlevel, reloading the configuration and setenv
 * with the old-style /dev/initctl FIFO.
 */
#define INIT_SOCKET             _PATH_VARRUN "finit.sock"
#define INIT_MAGIC              0x03091969

#define INIT_CMD_START          0
#define INIT_CMD_RUNLVL         1
#define INIT_CMD_POWERFAIL      2
#define INIT_CMD_POWERFAILNOW   3
#define INIT_CMD_POWEROK        4
#define INIT_CMD_BSD            5
#define INIT_CMD_SETENV         6
#define INIT_CMD_UNSETENV       7

/* Finit extensions over std SysV */
#define INIT_CMD_DEBUG          8    /* Toggle Finit debug */
#define INIT_CMD_RELOAD         9    /* Reload *.conf in /etc/finit.d/ */
#define INIT_CMD_START_SVC      10
#define INIT_CMD_STOP_SVC       11
#define INIT_CMD_RELOAD_SVC     12   /* SIGHUP service */
#define INIT_CMD_RESTART_SVC    13   /* STOP + START service */
#define INIT_CMD_QUERY_INETD    14
#define INIT_CMD_UNUSED1        15   /* Unused, was INIT_CMD_EMIT */
#define INIT_CMD_GET_RUNLEVEL   16
#define INIT_CMD_WDOG_HELLO     128  /* Watchdog register and hello */
#define INIT_CMD_SVC_ITER       129
#define INIT_CMD_SVC_QUERY      130
#define INIT_CMD_SVC_FIND       131
#define INIT_CMD_NACK           254
#define INIT_CMD_ACK            255

/* Traditionally aligned on 384 bytes */
struct init_request {
	int	magic;		/* Magic number			*/
	int	cmd;		/* What kind of request		*/
	int	runlevel;	/* Runlevel to change to	*/
	int	sleeptime;	/* Time between TERM and KILL	*/
	char	data[368];
};

extern int    runlevel;
extern int    cfglevel;
extern int    prevlevel;
extern int    rescue;
extern int    single;
extern int    splash;
extern char  *rcsd;
extern char  *sdown;
extern char  *network;
extern char  *hostname;
extern char  *runparts;
extern uev_ctx_t *ctx;

#endif /* FINIT_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
