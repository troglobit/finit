/* Save and restore RTC using hwclock
 *
 * Copyright (c) 2012-2019  Joachim Nilsson <troglobit@gmail.com>
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

#include <time.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#include <lite/lite.h>

#include "finit.h"
#include "helpers.h"
#include "plugin.h"

/* Kernel RTC driver validates against this date for sanity check */
#define RTC_TIMESTAMP_BEGIN_2000        946684800LL /* 2000-01-01 00:00:00 */

static void tz_set(char *tz, size_t len)
{
	char *ptr;

	ptr = getenv("TZ");
	if (!ptr)
		memset(tz, 0, len);
	else
		strlcpy(tz, ptr, len);

	setenv("TZ", "UTC0", 1);
	tzset();
}

static void tz_restore(char *tz)
{
	if (!tz || !tz[0])
		unsetenv("TZ");
	else
		setenv("TZ", tz, 1);
	tzset();
}

static int rtc_open(void)
{
	char *alt[] = {
		"/dev/rtc",
		"/dev/rtc0",
		"/dev/misc/rtc"
	};
	size_t i;
	int fd;

	for (i = 0; i < NELEMS(alt); i++) {
		fd = open(alt[i], O_RDONLY);
		if (fd < 0)
			continue;

		return fd;
	}

	return -1;
}

static void rtc_save(void *arg)
{
	struct timeval tv = { 0 };
	struct tm tm = { 0 };
	int fd, rc = 0;
	char tz[128];

	fd = rtc_open();
	if (fd < 0)
		return;

	tz_set(tz, sizeof(tz));
	rc = gettimeofday(&tv, NULL);
	if (rc < 0 || tv.tv_sec < RTC_TIMESTAMP_BEGIN_2000) {
		print_desc(NULL, "System clock invalid, not saving to RTC");
	invalid:
		logit(LOG_ERR, "System clock invalid, before 2000-01-01 00:00, not saving to RTC");
		rc = 2;
		goto out;
	}

	print_desc(NULL, "Saving system time (UTC) to RTC");

	gmtime_r(&tv.tv_sec, &tm);
	if (ioctl(fd, RTC_SET_TIME, &tm) < 0) {
		if (EINVAL == errno)
			goto invalid;
		rc = 1;
		goto out;
	}

out:
	tz_restore(tz);
	print(rc, NULL);
	close(fd);
}

static void rtc_restore(void *arg)
{
	struct timeval tv = { 0 };
	struct tm tm = { 0 };
	int fd, rc = 0;
	char tz[128];

	fd = rtc_open();
	if (fd < 0)
		return;

	tz_set(tz, sizeof(tz));
	if (ioctl(fd, RTC_RD_TIME, &tm) < 0) {
		char msg[120];

		snprintf(msg, sizeof(msg), "Failed restoring system clock, %s",
			 EINVAL == errno ? "RTC time is too old"   :
			 ENOENT == errno ? "RTC has no saved time" : "see log for details");
		print_desc(NULL, msg);

	invalid:
		logit(LOG_ERR, "Failed restoring system clock from RTC.");
		if (EINVAL == errno)
			logit(LOG_ERR, "RTC time is too old (before 2000-01-01 00:00)");
		else if (ENOENT == errno)
			logit(LOG_ERR, "RTC has no previously saved (valid) time.");
		else
			logit(LOG_ERR, "RTC error code %d: %m", errno);
		rc = 2;
		goto out;
	}

	print_desc(NULL, "Restoring system clock (UTC) from RTC");
	tm.tm_isdst = -1; /* Use tzdata to figure it out, please. */
	tv.tv_sec = mktime(&tm);
	if (tv.tv_sec < RTC_TIMESTAMP_BEGIN_2000) {
		errno = EINVAL;
		goto invalid;
	}

	if (settimeofday(&tv, NULL) == -1)
		rc = 1;

out:
	tz_restore(tz);
	print(rc, NULL);
	close(fd);
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = {
		.cb  = rtc_restore
	},
	.hook[HOOK_SHUTDOWN] = {
		.cb  = rtc_save
	}
};

PLUGIN_INIT(plugin_init)
{
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
