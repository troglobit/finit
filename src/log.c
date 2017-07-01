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

#include <stdio.h>
#include <stdarg.h>
#include <lite/lite.h>

#include "finit.h"
#include "log.h"


/*
 * Log to /dev/kmsg until syslogd has started, then openlog()
 * and continue logging as a regular daemon.
 */
void logit(int prio, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (!_slup && !fexist("/dev/log")) {
	    FILE *fp;

	    fp = fopen("/dev/kmsg", "w");
	    if (fp) {
		    if (debug)
			    prio = LOG_ERR;

		    fprintf(fp, "<%d>finit[1]:", LOG_DAEMON | prio);
		    vfprintf(fp, fmt, ap);
		    fclose(fp);
	    } else {
		    fprintf(stderr, "Failed opening /dev/kmsg for appending ...\n");
		    if (debug || prio <= LOG_ERR)
			    vfprintf(stderr, fmt, ap);
	    }
	    va_end(ap);
	    return;
    }

    if (!_slup) {
	    openlog("finit", LOG_PID, LOG_DAEMON);
	    _slup = 1;
    }

    vsyslog(prio, fmt, ap);
    va_end(ap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
