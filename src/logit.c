/*
 * Copyright (c) 2018-2025  Joachim Wiberg <troglobit@gmail.com>
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
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#define LOG_MAX (200 * 1024)
#define LOG_NUM 5

static const char version_info[] = PACKAGE_NAME " v" PACKAGE_VERSION;
extern int logrotate(char *file, int num, off_t sz);


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
		fsync(fileno(fp));
		if (checksz(fp, sz))
			return logrotate(logfile, num, sz);
	} else {
		while ((fgets(buf, len, stdin))) {
			fputs(buf, fp);
			fsync(fileno(fp));

			if (checksz(fp, sz)) {
				logrotate(logfile, num, sz);
				buf[0] = 0;
				goto reopen;
			}
		}
	}

	return fclose(fp);
}

/*
 * Parse possible systemd style log <level> prefix from message
 * For details, see libsystemd/sd-daemon.h
 *
 * Returns the level, and updates buf to point past the prefix
 */
static int parse_level(char **buf, int default_level)
{
	char *msg = *buf;

	if (msg[0] == '<' && msg[2] == '>' && msg[1] >= '0' && msg[1] <= '7') {
		int level;

		switch (msg[1]) {
		case '0': level = LOG_EMERG;    break;
		case '1': level = LOG_ALERT;    break;
		case '2': level = LOG_CRIT;     break;
		case '3': level = LOG_ERR;      break;
		case '4': level = LOG_WARNING;  break;
		case '5': level = LOG_NOTICE;   break;
		case '6': level = LOG_INFO;     break;
		case '7': level = LOG_DEBUG;    break;
		default:  level = default_level; break;
		}

		*buf = msg + 3;
		return (default_level & ~LOG_PRIMASK) | level;
	}

	return default_level;
}

static int do_log(int level, char *msg)
{
	level = parse_level(&msg, level);
	syslog(level, "%s", msg);

	return 0;
}

static int logit(int level, char *buf, size_t len)
{
	if (buf[0])
		return do_log(level, buf);

	while ((fgets(buf, len, stdin)))
		do_log(level, buf);

	return 0;
}

static int parse_prio(char *arg, int *fac, int *lvl)
{
	char *duparg = strdup(arg);
	const char *prio;
	char *ptr;
	int found;

	if (!duparg)
		prio = arg;
	else
		prio = duparg;

	ptr = strchr(prio, '.');
	if (ptr) {
		*ptr++ = 0;

		found = -1;
		for (int i = 0; facilitynames[i].c_name; i++) {
			if (!strcmp(facilitynames[i].c_name, prio)) {
				found = facilitynames[i].c_val;
				break;
			}
		}

		if (found == -1)
			goto notfound;

		*fac = found;
		prio = ptr;
	}

	found = -1;
	for (int i = 0; prioritynames[i].c_name; i++) {

		if (!strcmp(prioritynames[i].c_name, prio)) {
			found = prioritynames[i].c_val;
			break;
		}
	}

	if (found == -1) {
	notfound:
		if (duparg)
			free(duparg);
		return 1;
	}
	*lvl = found;

	if (duparg)
		free(duparg);

	return 0;
}

static int usage(int code)
{
	fprintf(stderr, "Usage: logit [OPTIONS] [MESSAGE]\n"
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
		"  -v       Show program version\n"
		"\n"
		"This version of logit is distributed as part of Finit.\n"
		"Bug report address: %s\n", PACKAGE_BUGREPORT);

	return code;
}

int main(int argc, char *argv[])
{
	char *ident = NULL, *logfile = NULL;
	int log_opts = LOG_NOWAIT;
	int facility = LOG_USER;
	int level = LOG_INFO;
	off_t size = LOG_MAX;
	char buf[512] = "";
	int num = LOG_NUM;
	int c, rc;

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

	if (!ident) {
		ident = getenv("LOGNAME");
		if (!ident)
			ident = getenv("USER");
		if (!ident)
			ident = PACKAGE_NAME;
	}

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
