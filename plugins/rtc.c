/* Save and restore RTC using hwclock
 *
 * Copyright (c) 2012-2024  Joachim Wiberg <troglobit@gmail.com>
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

#include "config.h"

#include <time.h>
#include <sys/ioctl.h>
#include <linux/rtc.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "finit.h"
#include "helpers.h"
#include "plugin.h"

/* Kernel RTC driver validates against this date for sanity check */
#define RTC_TIMESTAMP_BEGIN_2000 "2000-01-01 00:00:00"
#ifdef  RTC_TIMESTAMP_CUSTOM
static char  *rtc_timestamp     = RTC_TIMESTAMP_CUSTOM;
#else
static char  *rtc_timestamp     = RTC_TIMESTAMP_BEGIN_2000;
#endif
static time_t rtc_date_fallback = 946684800LL;

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

static int time_set(struct tm *tm)
{
	struct tm fallback = { 0 };
	struct timeval tv = { 0 };
	char tz[128];
	int rc = 0;

	tz_set(tz, sizeof(tz));

	if (!tm) {
		logit(LOG_NOTICE, "Resetting system clock to kernel default, %s.", rtc_timestamp);
		tm = &fallback;

		/* Attempt to set RTC to a sane value ... */
		tv.tv_sec = rtc_date_fallback;
		if (!gmtime_r(&tv.tv_sec, tm)) {
			rc = 1;
			goto out;
		}
	}

	tm->tm_isdst = -1; /* Use tzdata to figure it out, please. */
	tv.tv_sec = mktime(tm);
	if (tv.tv_sec == (time_t)-1 || tv.tv_sec < rtc_date_fallback) {
		errno = EINVAL;
		rc = 2;
	} else {
		if (settimeofday(&tv, NULL) == -1)
			rc = 1;
	}
out:
	tz_restore(tz);
	return rc;
}

static int time_get(struct tm *tm)
{
	struct timeval tv = { 0 };
	char tz[128];
	int rc = 0;

	tz_set(tz, sizeof(tz));

	rc = gettimeofday(&tv, NULL);
	if (rc < 0 || tv.tv_sec < rtc_date_fallback)
		rc = 2;
	else
		gmtime_r(&tv.tv_sec, tm);

	tz_restore(tz);

	return rc;
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
	struct tm tm = { 0 };
	int fd, rc = 0;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	fd = rtc_open();
	if (fd < 0)
		return;

	if ((rc = time_get(&tm))) {
		print_desc(NULL, "System clock invalid, not saving to RTC");
	} else {
		print_desc(NULL, "Saving system clock (UTC) to RTC");
		rc = ioctl(fd, RTC_SET_TIME, &tm);
	}

	if (rc && errno == EINVAL) {
		logit(LOG_ERR, "System clock invalid, before %s, not saving to RTC", rtc_timestamp);
		rc = 2;
	}

	print(rc, NULL);
	close(fd);
}

static void rtc_restore(void *arg)
{
	struct tm tm = { 0 };
	int fd, rc = 0;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	fd = rtc_open();
	if (fd < 0) {
		logit(LOG_NOTICE, "System has no RTC (missing driver?), skipping restore.");
		return;
	}

	if ((rc = ioctl(fd, RTC_RD_TIME, &tm)) < 0) {
		char msg[120];

		snprintf(msg, sizeof(msg), "Failed restoring system clock, %s",
			 EINVAL == errno ? "RTC time is too old"   :
			 ENOENT == errno ? "RTC has no saved time" : "see log for details");
		print_desc(NULL, msg);
	} else {
		print_desc(NULL, "Restoring system clock (UTC) from RTC");
		rc = time_set(&tm);
	}

	if (rc) {
		logit(LOG_ERR, "Failed restoring system clock from RTC.");
		if (EINVAL == errno)
			logit(LOG_ERR, "RTC time is too old (before %s)", rtc_timestamp);
		else if (ENOENT == errno)
			logit(LOG_ERR, "RTC has no previously saved (valid) time.");
		else
			logit(LOG_ERR, "RTC error code %d: %s", errno, strerror(errno));

		time_set(NULL);
		rc = 2;
	}

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
	struct tm tm = { 0 };

	if (!strptime(rtc_timestamp, "%Y-%m-%d %H:%M", &tm))
		rtc_date_fallback = mktime(&tm);
	else
		rtc_timestamp = RTC_TIMESTAMP_BEGIN_2000;

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
