/* UTMP API
 *
 * Copyright (c) 2016-2024  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_UTMP_API_H_
#define FINIT_UTMP_API_H_

#include <paths.h>
#include <utmp.h>

#ifndef _PATH_BTMP
#define _PATH_BTMP "/var/log/btmp"
#endif

int utmp_set         (int type, int pid, char *line, char *id, char *user);
int utmp_set_boot    (void);
int utmp_set_halt    (void);
int utmp_set_init    (char *tty, char *id);
int utmp_set_login   (char *tty, char *id);
int utmp_set_dead    (int pid);
int utmp_set_runlevel(int pre, int now);
int utmp_show        (char *file);

void runlevel_set    (int pre, int now);

/*
 * musl libc default to /dev/null/utmp and /dev/null/wtmp, respectively.
 * See https://www.openwall.com/lists/musl/2012/03/04/4 for reasoning.
 * Also, there's no __MUSL__, so we cannot make a libc-specific #ifdef
 */
static inline int has_utmp(void)
{
	return strncmp(_PATH_UTMP, "/dev/null", 9);
}

#endif /* FINIT_UTMP_API_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
