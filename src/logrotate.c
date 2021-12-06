/*
 * Copyright (c) 2018-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

static int recreate(char *path, mode_t mode, uid_t uid, gid_t gid)
{
	if (mknod(path, S_IFREG | mode, 0) || chown(path, uid, gid)) {
		syslog(LOG_ERR, "Failed recreating %s: %s", path, strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * This function triggers a log rotates of @file when size >= @sz bytes
 * At most @num old versions are kept and by default it starts gzipping
 * .2 and older log files.  If gzip is not available in $PATH then @num
 * files are kept uncompressed.
 */
int logrotate(char *file, int num, off_t sz)
{
	int cnt;
	struct stat st;

	if (stat(file, &st))
		return 1;

	if (sz > 0 && S_ISREG(st.st_mode) && st.st_size > sz) {
		if (num > 0) {
			size_t len = strlen(file) + 10 + 1;
			char   ofile[len];
			char   nfile[len];

			/* First age zipped log files */
			for (cnt = num; cnt > 2; cnt--) {
				snprintf(ofile, len, "%s.%d.gz", file, cnt - 1);
				snprintf(nfile, len, "%s.%d.gz", file, cnt);

				/* May fail because ofile doesn't exist yet, ignore. */
				if (rename(ofile, nfile) && errno != ENOENT)
					syslog(LOG_ERR, "Failed logrotate %s: %s",
					       ofile, strerror(errno));
			}

			for (cnt = num; cnt > 0; cnt--) {
				snprintf(ofile, len, "%s.%d", file, cnt - 1);
				snprintf(nfile, len, "%s.%d", file, cnt);

				/* May fail because ofile doesn't exist yet, ignore. */
				if (rename(ofile, nfile) && errno != ENOENT) {
					syslog(LOG_ERR, "Failed logrotate %s: %s",
					       ofile, strerror(errno));
					continue;
				}

				if (cnt == 2 && fexist(nfile)) {
					if (systemf("gzip %s 2>/dev/null", nfile))
						continue; /* no gzip, probably */

					(void)remove(nfile);
				}
			}

			if (rename(file, nfile))
				goto fallback;
			recreate(file, st.st_mode, st.st_uid, st.st_gid);
		} else {
		fallback:
			if (truncate(file, 0))
				syslog(LOG_ERR, "Failed truncating %s during logrotate: %s",
				       file, strerror(errno));
		}
	}

	return 0;
}
