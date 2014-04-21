/* Collection of utility funcs and C library extensions for finit & its plugins
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

#ifndef FINIT_LITE_H_
#define FINIT_LITE_H_

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h> /* MAX(), isset(), setbit(), TRUE, FALSE, et consortes. :-) */
#include <unistd.h>

int     fexist     (char *file);
int     fisdir     (char *file);
mode_t  fmode      (char *file);

ssize_t copyfile   (char *src, char *dst, int len, int sym);
int     movefile   (char *src, char *dst);
int     copy_filep (FILE *src, FILE *dst);
size_t  fsendfile  (FILE *out, FILE *in, size_t sz);

int     dir        (const char *dir, const char *type, int (*filter) (const char *file), char ***list, int strip);
int     rsync      (char *src, char *dst, int delete, int (*filter) (const char *file));

#ifndef strlcpy
size_t  strlcpy    (char *dst, const char *src, size_t siz);
#endif
#ifndef strlcat
size_t  strlcat    (char *dst, const char *src, size_t siz);
#endif
#ifndef strtonum
long long strtonum (const char *numstr, long long minval, long long maxval, const char **errstrp);
#endif

static inline int fisslashdir(char *dir)
{
   if (!dir)             return 0;
   if (strlen (dir) > 0) return dir[strlen (dir) - 1] == '/';
                         return 0;
}


/* Validate string, non NULL and not zero length */
static inline int string_valid (const char *s)
{
   return s && strlen (s);
}

/* Relaxed comparison, e.g., sys_string_match("small", "smaller") => TRUE */
static inline int string_match (const char *a, const char *b)
{
   size_t min = MIN(strlen (a), strlen (b));

   return !strncasecmp (a, b, min);
}

/* Strict comparison, e.g., sys_string_match("small", "smaller") => FALSE */
static inline int string_compare (const char *a, const char *b)
{
   return strlen (a) == strlen (b) && !strcmp (a, b);
}

/* Strict comparison, like sys_string_compare(), but case insensitive,
 * e.g., sys_string_match("small", "SmAlL") => TRUE
 */
static inline int string_case_compare (const char *a, const char *b)
{
   return strlen (a) == strlen (b) && !strcasecmp (a, b);
}

#endif /* FINIT_LITE_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */

