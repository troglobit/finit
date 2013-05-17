/* Functions for operating on files in directories.
 *
 * Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
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

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *matcher_type = NULL;
static int (*matcher_filter) (const char *file) = NULL;
static int matcher(const struct dirent *entry)
{
	char *pos = strrchr(entry->d_name, '.');

	if (matcher_filter && !matcher_filter(entry->d_name))
		/* User matcher overrides the rest. */
		return 0;

	/* Skip current dir "." from list of files. */
	if ((1 == strlen(entry->d_name) && entry->d_name[0] == '.') ||
	    (2 == strlen(entry->d_name) && !strcmp(entry->d_name, "..")))
		return 0;

	/* filetype == "" */
	if (matcher_type[0] == 0)
		return 1;

	/* Entry has no "." */
	if (!pos)
		return 0;

	return !strcmp(pos, matcher_type);
}

/**
 * dir - List all files of a certain type in the given directory.
 * @dir:   Base directory for dir operation.
 * @type:  File type suffix, e.g. ".cfg".
 * @filter: Optional file name filter.
 * @list:  Pointer to an array of file names.
 * @strip: Flag, if set dir() strips the file type.
 *
 * This function returns a @list of files, matching the @type suffix,
 * in the given directory @dir.
 *
 * The @list argument is a pointer to where to store the dynamically
 * allocated list of file names.  This list should be free'd by first
 * calling free() on each file name and then on the list itself.
 *
 * If @filter is not %NULL it will be called for each file found.  If
 * @filter returns non-zero the @file argument will be included in the
 * resulting @list.  If @filter returns zero for given @file it will
 * be discarded.
 *
 * If the @strip flag is set the resulting @list of files has their
 * file type stripped, including the dot.  So a match "config0.cfg"
 * would be returned as "config0".
 *
 * Returns:
 * Number of files in @list, zero if no matching files of @type.
 */
int dir(const char *dir, const char *type, int (*filter) (const char *file), char ***list, int strip)
{
	int i, n, num = 0;
	char **files;
	struct dirent **namelist;

	assert(list);

	if (!dir)
		/* Assuming current directory */
		dir = ".";
	if (!type)
		/* Assuming all files. */
		type = "";

	matcher_type = type;
	matcher_filter = filter;
	n = scandir(dir, &namelist, matcher, alphasort);
	if (n < 0) {
		perror("scandir");
	} else if (n > 0) {
		files = (char **)malloc(n * sizeof(char *));
		for (i = 0; i < n; i++) {
			if (files) {
				char *name = namelist[i]->d_name;
				char *type = strrchr(name, '.');

				if (type && strip)
					*type = 0;

				files[i] = strdup(name);
				num++;
			}
			free(namelist[i]);
		}
		if (num)
			*list = files;
	}

	if (namelist)
		free(namelist);

	return num;
}

#ifdef UNITTEST
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "otn/c.h"

#define DIR_TYPE_IMAGE  ".img"
#define DIR_TYPE_SYSLOG ""
#define DIR_TYPE_CONFIG ".cfg"
#define STARTUP_CONFIG  "startup-config.cfg"

int simulate_files(int creat)
{
	int i;
	char *files[] =
	    { "config0.cfg", "config1.cfg", "config2.cfg", "config3.cfg",
		"rr109.img", "rx100.img", "rx107.img", "rm957.img", "messages"
	};

	for (i = 0; i < ARRAY_ELEMENTS(files); i++) {
		if (creat)
			touch(files[i]);
		else
			remove(files[i]);
	}

	if (creat)
		symlink("config2.cfg", STARTUP_CONFIG);
	else
		remove(STARTUP_CONFIG);

	return 0;
}

static int cfg_dir_filter(const char *file)
{
	/* Skip the STARTUP_CONFIG file, it is a symbolic link to the
	 * current startup configuration. */
	return ! !strcmp(file, STARTUP_CONFIG);
}

int main(int argc, char *argv[])
{
	int i, num;
	char *type = DIR_TYPE_CONFIG;
	char **files;
	int once = 1;

	int is_startup_config(const char *entry) {
		static char file[80];

		if (once) {
			int len = readlink(STARTUP_CONFIG, file, sizeof(file));

			if (len == -1)
				return 0;

			file[len] = 0;
			once = 0;	/* Only once per call to dir() */
		}
		//printf ("Comparing link %s with entry %s\n", file, entry);
		return !strcmp(file, entry);
	}

	simulate_files(1);

	if (argc >= 2) {
		if (!strcasecmp("CONFIG", argv[1])) {
			type = DIR_TYPE_CONFIG;
			system("ls -l *" DIR_TYPE_CONFIG);
		}
		if (!strcasecmp("IMAGE", argv[1])) {
			type = DIR_TYPE_IMAGE;
			system("ls -l *" DIR_TYPE_IMAGE);
		}
		if (!strcasecmp("SYSLOG", argv[1])) {
			type = DIR_TYPE_SYSLOG;
			system("ls -l *");
		}
	}

	num = dir(NULL, type, cfg_dir_filter, &files, 0);
	if (num) {
		for (i = 0; i < num; i++) {
			printf("%s", files[i]);
			if (is_startup_config(files[i]))
				printf(" --> startup-config");
			printf("\n");

			free(files[i]);
		}
		free(files);
	}

	simulate_files(0);

	return 0;
}
#endif				/* UNITTEST */

/**
 * Local Variables:
 *  compile-command: "gcc -g -DUNITTEST -o unittest dir.c"
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
