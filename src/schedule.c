/* Work queue helper functions
 *
 * Copyright (c) 2018-2024  Joachim Wiberg <troglobit@gmail.com>
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
#include "private.h"
#include "log.h"
#include "schedule.h"

#define SC_INIT 0x494E4954	/* "INIT", see ascii(7) */

/*
 * libuEv callback wrapper
 */
static void cb(uev_t *w, void *arg, int events)
{
	struct wq *work = (struct wq *)arg;

	if (UEV_ERROR == events) {
		dbg("%s(): spurious problem with schedule work timer, restarting.", __func__);
		uev_timer_start(w);
		return;
	}

	work->cb(work);
}

/*
 * Place work on event queue
 */
int schedule_work(struct wq *work)
{
	int msec;

	if (!work)
		return errno = EINVAL;

	msec = work->delay;
	if (work->init != SC_INIT) {
		work->init = SC_INIT;
		return uev_timer_init(ctx, &work->watcher, cb, work, msec, 0);
	}

	return uev_timer_set(&work->watcher, msec, 0);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
