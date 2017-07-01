/* Finit daemon log functions
 *
 * Copyright (c) 2008-2017  Joachim Nilsson <troglobit@gmail.com>
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
#include <stdio.h>
#include <stdarg.h>
#include <lite/lite.h>

#include "finit.h"
#include "log.h"

static int loglevel = LOG_NOTICE;

void log_toggle_debug(void)
{
	debug = !debug;
	if (debug)
		silent = 0;
	else
		silent = quiet ? 1 : SILENT_MODE;

	logit(LOG_NOTICE, "Debug mode %s", debug ? "enabled" : "disabled");
}

static void early_logit(int prio, const char *fmt, va_list ap)
{
	FILE *fp;

	fp = fopen("/dev/kmsg", "w");
	if (fp) {
		if (debug)
			prio = LOG_ERR;

		fprintf(fp, "<%d>finit[1]:", LOG_DAEMON | prio);
		vfprintf(fp, fmt, ap);
		fclose(fp);
	} else {
		if (debug || prio <= LOG_ERR)
			vfprintf(stderr, fmt, ap);
	}
}

/*
 * Log to /dev/kmsg until syslogd has started, then openlog()
 * and continue logging as a regular daemon.
 */
void logit(int prio, const char *fmt, ...)
{
	va_list ap;
	static int up = 0;

	va_start(ap, fmt);
	if (!up) {
		if (!fexist("/dev/log")) {
			early_logit(prio, fmt, ap);
			goto done;
		}

		openlog("finit", LOG_PID, LOG_DAEMON);
		setlogmask(LOG_UPTO(loglevel));
		up = 1;
	}

	vsyslog(prio, fmt, ap);
done:
	va_end(ap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
