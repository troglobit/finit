/* inotify watcher for files or directories
 *
 * Copyright (c) 2015-2016  Tobias Waldekranz <tobias@waldekranz.com>
 * Copyright (c) 2016-2022  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_IWATCH_H_
#define FINIT_IWATCH_H_

#include <stdint.h>
#ifdef _LIBITE_LITE
# include <libite/queue.h>	/* BSD sys/queue.h API */
#else
# include <lite/queue.h>	/* BSD sys/queue.h API */
#endif
#include <sys/inotify.h>

#ifndef IN_MASK_CREATE
#define IN_MASK_CREATE 0x10000000	/* since Linux 4.18 */
#endif

/*
 * Monitors changes to both directories and files by default, but only
 * add watcher once (IN_MASK_CREATE) to avoid clobbering any already
 * monitored paths.
 */
#define IWATCH_MASK (IN_CREATE | IN_DELETE | IN_MODIFY | IN_ATTRIB | IN_MOVE | IN_MASK_CREATE)

struct iwatch_path {
	TAILQ_ENTRY(iwatch_path) link;
	char *path;
	int wd;
};

struct iwatch {
	int fd;
	TAILQ_HEAD(, iwatch_path) iwp_list;
};


int  iwatch_init (struct iwatch *iw);
void iwatch_exit (struct iwatch *iw);

int  iwatch_add  (struct iwatch *iw, char *file, uint32_t mask);
int  iwatch_del  (struct iwatch *iw, struct iwatch_path *iwp);

struct iwatch_path *iwatch_find_by_wd   (struct iwatch *iw, int wd);
struct iwatch_path *iwatch_find_by_path (struct iwatch *iw, const char *path);

#endif /* FINIT_IWATCH_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
