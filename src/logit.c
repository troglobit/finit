/*
 * Copyright (c) 2018  Joachim Nilsson <troglobit@gmail.com>
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

#include <config.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <unistd.h>
#include <sys/stat.h>

static const char version_info[] = PACKAGE_NAME " v" PACKAGE_VERSION;


static int create(char *path, mode_t mode, uid_t uid, gid_t gid)
{
	return mknod(path, S_IFREG | mode, 0) || chown(path, uid, gid);
}

/*
 * This function triggers a log rotates of @file when size >= @sz bytes
 * At most @num old versions are kept and by default it starts gzipping
 * .2 and older log files.  If gzip is not available in $PATH then @num
 * files are kept uncompressed.
 */
static int logrotate(char *file, int num, off_t sz)
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
				rename(ofile, nfile);
			}

			for (cnt = num; cnt > 0; cnt--) {
				snprintf(ofile, len, "%s.%d", file, cnt - 1);
				snprintf(nfile, len, "%s.%d", file, cnt);

				/* May fail because ofile doesn't exist yet, ignore. */
				(void)rename(ofile, nfile);

				if (cnt == 2 && !access(nfile, F_OK)) {
					size_t len = 5 + strlen(nfile) + 1;
					char cmd[len];

					snprintf(cmd, len, "gzip %s", nfile);
					system(cmd);

					remove(nfile);
				}
			}

			rename(file, nfile);
			create(file, st.st_mode, st.st_uid, st.st_gid);
		} else {
			if (truncate(file, 0))
				syslog(LOG_ERR | LOG_PERROR, "Failed truncating %s during logrotate: %s", file, strerror(errno));
		}
	}

	return 0;
}

static int checksz(FILE *fp, off_t sz)
{
	struct stat st;

	if (sz <= 0)
		return 0;

	if (!fstat(fileno(fp), &st) && st.st_size > sz) {
		fclose(fp);
		return 1;
	}

	return 0;
}

static int flogit(char *logfile, int num, off_t sz, char *buf, size_t len)
{
	FILE *fp;

reopen:
	fp = fopen(logfile, "a");
	if (!fp) {
		syslog(LOG_ERR | LOG_PERROR, "Failed opening %s: %s", logfile, strerror(errno));
		return 1;
	}

	if (buf[0]) {
		fprintf(fp, "%s\n", buf);
		if (checksz(fp, sz))
			return logrotate(logfile, num, sz);
	} else {
		while ((fgets(buf, len, stdin))) {
			fputs(buf, fp);

			if (checksz(fp, sz)) {
				logrotate(logfile, num, sz);
				buf[0] = 0;
				goto reopen;
			}
		}
	}

	return fclose(fp);
}

static int logit(int level, char *buf, size_t len)
{
	if (buf[0]) {
		syslog(level, "%s", buf);
		return 0;
	}

	while ((fgets(buf, len, stdin)))
		syslog(level, "%s", buf);

	return 0;
}

static int parse_prio(char *arg, int *f, int *l)
{
	char *ptr;

	ptr = strchr(arg, '.');
	if (ptr) {
		*ptr++ = 0;

		for (int i = 0; facilitynames[i].c_name; i++) {
			if (!strcmp(facilitynames[i].c_name, arg)) {
				*f = facilitynames[i].c_val;
				break;
			}
		}

		arg = ptr;
	}

	for (int i = 0; prioritynames[i].c_name; i++) {
		if (!strcmp(prioritynames[i].c_name, arg)) {
			*l = prioritynames[i].c_val;
			break;
		}
	}

	return 0;
}

static int usage(int code)
{
	fprintf(stderr, "Usage: %s [OPTIONS] [MESSAGE]\n"
		"\n"
		"Write MESSAGE (or stdin) to syslog, or file (with logrotate)\n"
		"\n"
		"  -h       This help text\n"
		"  -p PRIO  Priority (numeric or facility.level pair)\n"
		"  -t TAG   Log using the specified tag (defaults to user name)\n"
		"  -s       Log to stderr as well as the system log\n"
		"\n"
		"  -f FILE  File to write log messages to, instead of syslog\n"
		"  -n SIZE  Number of bytes before rotating, default: 200 kB\n"
		"  -r NUM   Number of rotated files to keep, default: 5\n"
		"  -v        Show program version\n"
		"\n"
		"Bug report address: %s\n", PACKAGE, PACKAGE_BUGREPORT);

	return code;
}

int main(int argc, char *argv[])
{
	int c, rc, num = 5;
	int facility = LOG_USER;
	int level = LOG_INFO;
	int log_opts = LOG_NOWAIT;
	off_t size = 200 * 1024;
	char *ident = NULL, *logfile = NULL;
	char buf[512] = "";

	while ((c = getopt(argc, argv, "f:hn:p:r:st:v")) != EOF) {
		switch (c) {
		case 'f':
			logfile = optarg;
			break;

		case 'h':
			return usage(0);

		case 'n':
			size = atoi(optarg);
			break;

		case 'p':
			if (parse_prio(optarg, &facility, &level))
				return usage(1);
			break;

		case 'r':
			num = atoi(optarg);
			break;

		case 's':
			log_opts |= LOG_PERROR;
			break;

		case 't':
			ident = optarg;
			break;

		case 'v':	/* version */
			fprintf(stderr, "%s\n", version_info);
			return 0;

		default:
			return usage(1);
		}
	}

	if (!ident)
		ident = getenv("LOGNAME") ?: getenv("USER");

	if (optind < argc) {
		size_t pos = 0, len = sizeof(buf);

		while (optind < argc) {
			size_t bytes;

			bytes = snprintf(&buf[pos], len, "%s ", argv[optind++]);
			pos += bytes;
			len -= bytes;
		}
	}

	openlog(ident, log_opts, facility);

	if (logfile)
		rc = flogit(logfile, num, size, buf, sizeof(buf));
	else
		rc = logit(level, buf, sizeof(buf));

	closelog();

	return rc;
}
