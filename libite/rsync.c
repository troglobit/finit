/* Micro "rsync" implementation.
 *
 * Copyright (c) 2011, 2012  Joachim Nilsson <troglobit@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdlib.h>	/* NULL, free() */
#include <string.h>	/* strlen() */
#include <strings.h>	/* rindex() */
#include <stdio.h>
#include <sys/param.h>	/* MAX(), isset(), setbit(), TRUE, FALSE, et consortes. :-) */
#include <sys/stat.h>
#include <sys/types.h>

#include "lite.h"
#include "../helpers.h"

static int makedir(char *buf, size_t buf_len, char *dir, char *name, mode_t mode);
static int prune(char *dst, char **new_files, int new_num);


/**
 * rsync - Synchronize contents and optionally remove non-existing backups
 * @src: Source directory
 * @dst: Destination directory
 * @delete: Prune files from @dst that no longer exist in @src.
 * @filter: Optional filtering function for source directory.
 *
 * This is a miniature implementation of the famous rsync for local use only.
 * In fact, it is not even a true rsync since it copies all files from @src
 * to @dst.  The @delete option is useful for creating backups, when set all
 * files removed from src since last backup are pruned from the destination
 * (backup) directory.
 *
 * The filter callback, @filter, if provided, is used to determine what files to
 * include from the source directory when backing up.  If a file is to be skipped
 * the callback should simply return zero.
 *
 * Returns:
 * POSIX OK(0), or non-zero with @errno set on error.
 */
int rsync(char *src, char *dst, int delete, int (*filter) (const char *file))
{
	char source[256];
	char dest[256];
	int i = 0, num = 0, result = 0;
	char **files;		/* Array of file names. */

	void copy(char *src, char *dst)
	{
		errno = 0;
		copyfile(src, dst, 0);
		if (errno) result++;
	}

	if (!fisdir(dst))
		mkdir(dst, 0755);

	if (!fisdir(src)) {
		if (!fexist(src))
			return 1;

		copy(src, dst);
		return errno;
	}

	/* Copy dir as well? */
	if (!fisslashdir(src)) {
		char *ptr = rindex(src, '/');

		if (!ptr)
			ptr = src;
		else
			ptr++;

		if (makedir(dest, sizeof(dest), dst, ptr, fmode(src)))
			return 1;
		dst = dest;
	}

	num = dir(src, "", filter, &files, 0);
	for (i = 0; i < num; i++) {
		/* Recursively copy sub-directries */
		snprintf(source, sizeof(source), "%s%s%s", src, fisslashdir(src) ? "" : "/", files[i]);
		if (fisdir(source)) {
			char dst2[256];

			strcat(source, "/");
			if (makedir (dst2, sizeof(dst2), dst, files[i], fmode(source))) {
				result++;
				continue;
			}

			rsync(source, dst2, delete, filter);
			continue;	/* Next file/dir in @src to copy... */
		}
		copy(source, dst);
	}

	/* We ignore any errors from the pruning, that phase albeit useful is only
	 * cosmetic. --Jocke 2011-03-24 */
	if (delete)
		prune(dst, files, num);

	if (num) {
		for (i = 0; i < num; i++)
			free(files[i]);
		free(files);
	}

	return result;
}

/* Creates dir/name @mode ... skipping / if dir already ends so. */
static int makedir(char *buf, size_t buf_len, char *dir, char *name, mode_t mode)
{
	snprintf(buf, buf_len, "%s%s%s/", dir, fisslashdir(dir) ? "" : "/", name);
	if (mkdir(buf, mode))
		return 1;

	return 0;
}

/* Prune old files, no longer existing on source, from destination directory. */
static int prune(char *dst, char **new_files, int new_num)
{
	int i, num, result = 0;
	char **files;

	int find(char *file) {
		int n;

		for (n = 0; n < new_num; n++)
			if (!strncmp (new_files[n], file, MAX(strlen(new_files[n]), strlen(file))))
				return 1;

		return 0;
	}

	num = dir(dst, "", NULL, &files, 0);
	if (num) {
		for (i = 0; i < num; i++) {
			if (!find(files[i])) {
				char *name;
				size_t len = strlen(files[i]) + 2 + strlen(dst);

				name = malloc(len);
				if (name) {
					snprintf(name, len, "%s%s%s", dst, fisslashdir(dst) ? "" : "/", files[i]);
					if (remove(name))
						result++;
					free(name);
				}
			}
			free(files[i]);
		}
		free(files);
	}

	return result;
}

#ifdef UNITTEST
#define BASE "/tmp/.unittest/"
#define SRC  BASE "src/"
#define DST  BASE "dst/"

char *files[] = {
	SRC "sub1/1.tst",
	SRC "sub1/2.tst",
	SRC "sub1/3.tst",
	SRC "sub2/4.tst",
	SRC "sub2/5.tst",
	SRC "sub2/6.tst",
	SRC "sub3/7.tst",
	SRC "sub3/8.tst",
	SRC "sub3/9.tst",
	NULL
};

void cleanup_test(void)
{
	system("rm -rf " BASE);
}

void setup_test(void)
{
	int i;
	char cmd[256];
	mode_t dir_modes[] = { 755, 700 };
	mode_t file_modes[] = { 644, 600 };

	cleanup_test();

	mkdir(BASE, 0755);
	mkdir(SRC, 0755);
	mkdir(DST, 0755);

	for (i = 0; files[i]; i++) {
		snprintf(cmd, sizeof(cmd), "mkdir -m %d -p `dirname %s`",
			 dir_modes[i % 2], files[i]);
		system(cmd);

		snprintf(cmd, sizeof(cmd), "touch %s; chmod %d %s", files[i],
			 file_modes[i % 2], files[i]);
		system(cmd);
	}
}

int run_test()
{
#if 0
	setup_test();
	puts("Before:");
	system("tree -np " BASE);

	rsync(SRC, DST, 0, NULL);
	puts("After:");
	system("tree -np " BASE);
	cleanup_test();
#endif

	setup_test();
	rsync(BASE "src", DST, 0, NULL);
	puts("Only partial rsync of src <-- No slash!");
	system("tree -np " BASE);
#if 0
	cleanup_test();
	setup_test();
	rsync(BASE "src/sub1", BASE "dst", 0, NULL);
	puts("Only partial rsync of src/sub1 <-- No slashes!!");
	system("tree -np " BASE);

	cleanup_test();
	setup_test();
	rsync(BASE "src/sub1/", DST, 0, NULL);
	puts("Only partial rsync of src/sub1/");
	system("tree -np " BASE);

	cleanup_test();
	setup_test();
	rsync(BASE "src/sub1", DST, 0, NULL);
	puts("Only partial rsync of src/sub1 <-- No slash!");
	system("tree -np " BASE);

	rsync("/etc", "/var/tmp", 0, NULL);
	puts("Real life test:");
	system("tree -np /var/tmp");
#endif
	return 0;
}

int main(int argc, char *argv[])
{
	atexit(cleanup_test);

	return run_test();
}
#endif				/* UNITTEST */

/**
 * Local Variables:
 *  compile-command: "make -f rsync.mk"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
