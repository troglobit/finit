/* Condition engine (write)
 *
 * Copyright (c) 2015-2016  Tobias Waldekranz <tobias@waldekranz.com>
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

#include <libgen.h>
#include <lite/lite.h>
#include <stdio.h>
#include <utime.h>

#include "finit.h"
#include "cond.h"
#include "service.h"

int cond_set_path(const char *path, enum cond_state new)
{
	char buf[MAX_ARG_LEN], *dir;
	enum cond_state old;

	old = cond_get_path(path);

	switch (new) {
	case COND_ON:
		strlcpy(buf, path, sizeof(buf));
		dir = dirname(buf);
		if (!dir) {
			_e("Invalid path '%s' for condition", path);
			return 0;
		}
		if (makepath(dir) && errno != EEXIST) {
			_pe("Failed creating dir '%s' for condition '%s'", dir, path);
			return 0;
		}
		touch(path);
		utime(path, NULL);
		break;

	case COND_OFF:
		unlink(path);
		break;

	default:
		_e("Invalid condition state");
		return 0;
	}

	return new != old;
}

/* Has condition in configuration and cond is allowed? */
static int svc_has_cond(svc_t *svc)
{
	if (!svc->cond[0])
		return 0;

	switch (svc->type) {
	case SVC_TYPE_SERVICE:
	case SVC_TYPE_TASK:
	case SVC_TYPE_RUN:
		return 1;

	default:
		break;
	}

	return 0;
}

static void cond_update(const char *name)
{
	svc_t *svc;

	_d("%s", name);
	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (!svc_has_cond(svc) || !cond_affects(name, svc->cond))
			continue;

		_d("%s: match <%s> %s", name ?: "nil", svc->cond, svc->cmd);
		service_step(svc);
	}
}

void cond_set(const char *name)
{	
	_d("%s", name);
	if (!cond_set_path(cond_path(name), COND_ON))
		return;

	cond_update(name);
}

void cond_clear(const char *name)
{
	_d("%s", name);
	if (!cond_set_path(cond_path(name), COND_OFF))
		return;

	cond_update(name);
}

void cond_reload(void)
{
	_d("");
	cond_set_path(COND_RECONF, COND_ON);

	cond_update(NULL);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
