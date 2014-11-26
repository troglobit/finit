/* Fastinit (finit) copyfile() implementation.
 *
 * Copyright (c) 2008 Claudio Matsuoka <http://helllabs.org/finit/>
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h> /* MAX(), isset(), setbit(), TRUE, FALSE, et consortes. :-) */
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "lite.h"

static int adjust_target(char *src, char **dst);

/**
 * copyfile - Copy a file to another.
 * @src: Source file.
 * @dst: Target file.
 * @len: Number of bytes to copy, zero (0) for entire file.
 * @sym: Should symlinks be recreated (1) or followed (0)
 *
 * This is a C implementation of the command line cp(1) utility.  It is one
 * of the classic missing links in the UNIX C library.  This version is from
 * the finit project, http://helllabs.org/finit/, which is a reimplementation
 * of fastinit for the Asus EeePC.
 *
 * Only some small changes to save stack space have been introduced.
 *
 * Returns:
 * The number of bytes copied, zero may be error (check errno!), but it
 * may also indicate that @src was empty. If @src is a directory errno
 * will be set to %EISDIR since copyfile() is not recursive.
 */
ssize_t copyfile(char *src, char *dst, int len, int sym)
{
	char *buffer;
	int in, out, isdir = 0, saved_errno = 0;
	ssize_t num, size = 0;

	errno = 0;

	buffer = malloc(BUFSIZ);
	if (!buffer)
		return 0;

	if (fisdir(src)) {
		saved_errno = EISDIR;	/* Error: source is a directory */
		goto exit;
	}

	/* Check if target is a directory, then append src filename. */
	isdir = adjust_target(src, &dst);

	if (sym) {
		size = readlink(src, buffer, BUFSIZ);
		if (size > 0) {
			if (size >= (ssize_t) BUFSIZ) {
				saved_errno = ENOBUFS;
				goto exit;
			}

			buffer[size] = 0;
			size = !symlink(buffer, dst);
			goto exit;
		}
	}

	/* 0: copy entire file */
	if (len == 0)
		len = INT_MAX; /* XXX: 2047 MiB should sufficient ... */

	in = open(src, O_RDONLY);
	if (in < 0) {
		saved_errno = errno;
		goto exit;
	}

	out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, fmode(src));
	if (out < 0) {
		close(in);
		saved_errno = errno;
		goto exit;
	}

	do {
		int clen = len > BUFSIZ ? BUFSIZ : len;

		num = read(in, buffer, clen);
		if (num > 0)
			size += write(out, buffer, num);
		len -= clen;
	} while (len > 0 && num == BUFSIZ);

	close(out);
	close(in);

exit:
	free(buffer);
	if (isdir)
		free(dst);
	errno = saved_errno;

	return size;
}

/**
 * movefile - Move a file to another location
 * @src: Source file.
 * @dst: Target file, or location.
 *
 * This is a C implementation of the command line mv(1) utility.
 * Usually the rename() API is sufficient, but not when moving across
 * file system boundaries.
 *
 * The @src argument must include the full path to the source file,
 * whereas the @dst argument may only be a directory, in which case the
 * same file name from @src is used.
 *
 * Returns:
 * POSIX OK(0), or non-zero with errno set.
 */
int movefile(char *src, char *dst)
{
	int isdir, result = 0;

	/* Check if target is a directory, then append the src base filename. */
	isdir = adjust_target(src, &dst);

	if (rename(src, dst)) {
		if (errno == EXDEV) {
			errno = 0;
			copyfile(src, dst, 0, 0);
			if (errno)
				result = 1;
			else
				result = remove(src);
		} else {
			result = 1;
		}
	}

	if (isdir)
		free(dst);

	return result;
}

/**
 * copy_filep - Copy between FILE *fp.
 * @src: Source FILE.
 * @dst: Destination FILE.
 *
 * Function takes signals into account and will restart the syscalls as long
 * as error is %EINTR.
 *
 * Returns:
 * POSIX OK(0), or non-zero with errno set on error.
 */
int copy_filep(FILE *src, FILE *dst)
{
	int result = 0;
	char *buf;

	if (!src || !dst)
		return EINVAL;

	buf = (char *)malloc(BUFSIZ);
	if (!buf)
		return errno;

	while (1) {
		errno = 0;
		if (!fgets(buf, BUFSIZ, src)) {
			if (errno == EINTR) {
				clearerr(src);
				continue;
			}

			if (feof(src))
				break;

			result = errno;
			goto exit;
		}

		while (EOF == fputs(buf, dst)) {
			if (errno == EINTR) {
				clearerr(dst);
				continue;
			}

			result = errno;
			goto exit;
		}
	}

exit:
	free(buf);
	return errno = result;
}

/**
 * fsendfile - copy data between file streams
 * @out: Destination stream
 * @in:  Source stream
 * @sz:  Size in bytes to copy.
 *
 * @out may be NULL, in which case @sz bytes are read and discarded
 * from @in. This is useful for streams where seeks are not
 * permitted. Additionally @sz may be the special value zero (0) in
 * which case fsendfile will copy until EOF is seen on @src.
 *
 * Returns:
 * The number of bytes copied. If there was an error, -1 is returned
 * and errno will be set accordingly.
 */
size_t fsendfile (FILE *out, FILE *in, size_t sz)
{
	char *buf;
	size_t blk = BUFSIZ, num = 0, tot = 0;

	buf = (char *)malloc(BUFSIZ);
	if (!buf)
		return -1;

	while (!sz || tot < sz) {
		if (sz && ((sz - tot) < BUFSIZ))
			blk = sz - tot;

		num = fread(buf, 1, blk, in);
		if (num == 0)
			break;

		if (out && (fwrite(buf, num, 1, out) != 1)) {
			num = -1;
			break;
		}

		tot += num;
	}

	free(buf);

	return (num == (size_t)-1) ? (size_t)-1 : tot;
}

/* Tests if dst is a directory, if so, reallocates dst and appends src filename returning 1 */
static int adjust_target(char *src, char **dst)
{
	int isdir = 0;

	if (fisdir(*dst)) {
		int slash = 0;
		char *tmp, *ptr = strrchr(src, '/');

		if (!ptr)
			ptr = src;
		else
			ptr++;

		tmp = malloc(strlen(*dst) + strlen(ptr) + 2);
		if (!tmp) {
			errno = EISDIR;
			return 0;
		}

		isdir = 1;	/* Free dst before exit! */
		slash = fisslashdir(*dst);

		sprintf(tmp, "%s%s%s", *dst, slash ? "" : "/", ptr);
		*dst = tmp;
	}

	return isdir;
}

#ifdef UNITTEST
#include <stdio.h>
/* XXX: Make a real set of unit tests ... */
#if 0
int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr,
			"Usage: %s <src> <dst>\nCopies the <src> file to the <dst> file.\n",
			argv[0]);
		return 1;
	}

	printf("Copying file %s to %s\n", argv[1], argv[2]);

	return copyfile(argv[1], argv[2], 0, 0);
}
#else
int main(void)
{
	int i = 0;
	char *files[] = {
		"/etc/passwd", "/tmp/tok", "/tmp/tok",
		"/etc/passwd", "/tmp/", "/tmp/passwd",
		"/etc/passwd", "/tmp", "/tmp/passwd",
		NULL
	};
	FILE *src, *dst;

	fprintf(stderr, "\n=>Start testing copy_filep()\n");
	while (files[i]) {
		fprintf(stderr, "copy_filep(%s, %s)\t", files[i],
			files[i + 1]);
		src = fopen(files[i], "r");
		dst = fopen(files[i + 1], "w");
		if (copy_filep(src, dst))
			perror("Failed");

		if (src)
			fclose(src);
		if (dst)
			fclose(dst);

		if (fexist(files[i + 2]))
			fprintf(stderr, "OK => %s\n", files[i + 2]);

		erase(files[i + 2]);
		i += 3;
	}

	printf("\n\n=>Start testing copyfile()\n");
	i = 0;
	while (files[i]) {
		fprintf(stderr, "copyfile(%s, %s)\t", files[i], files[i + 1]);
		if (!copyfile(files[i], files[i + 1], 0, 0))
			perror("Failed");

		if (fexist(files[i + 2]))
			fprintf(stderr, "OK => %s\n", files[i + 2]);

		erase(files[i + 2]);
		i += 3;
	}

	return 0;
}
#endif
#endif				/* UNITTEST */

/**
 * Local Variables:
 *  compile-command: "gcc -g -I../include -o unittest -DUNITTEST copyfile.c && ./unittest"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
