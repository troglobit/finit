/* New client tool, replaces old /dev/initctl API and telinit tool
 *
 * Copyright (c) 2015-2025  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_INITCTL_H_
#define FINIT_INITCTL_H_

#include <err.h>

#include "finit.h"
#include "util.h"

extern char *finit_conf;
extern char *finit_rcsd;

extern int icreate;			/* initctl -c */
extern int iforce;			/* initctl -f */
extern int ionce;			/* initctl -1 */
extern int heading;			/* initctl -t */
extern int json;			/* initctl -j */
extern int noerr;			/* initctl -n */
extern int verbose;			/* initctl -v */
extern int plain;			/* initctl -p */
extern int quiet;			/* initctl -q */

#define ERR(rc, fmt, args...)  do { if (!quiet) err(rc, fmt, ##args);  else exit(rc); } while (0)
#define ERRX(rc, fmt, args...) do { if (!quiet) errx(rc, fmt, ##args); else exit(rc); } while (0)
#define WARN(fmt, args...)     do { if (!quiet) warn(fmt, ##args);  } while (0)
#define WARNX(fmt, args...)    do { if (!quiet) warnx(fmt, ##args); } while (0)

extern void print_header(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

#endif /* FINIT_INITCTL_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
