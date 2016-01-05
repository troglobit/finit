/* Event aggregator, also serves as event cache, remembering GW and IFUP states
 *
 * Copyright (c) 2015  Joachim Nilsson <troglobit@gmail.com>
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

#include <libite/lite.h>
#include <stdio.h>

#include "finit.h"
#include "cond.h"
#include "service.h"

static inline int timespec_newer(const struct timespec *a,
				 const struct timespec *b)
{
	if (a->tv_sec != b->tv_sec)
		return a->tv_sec > b->tv_sec;

	return a->tv_nsec > b->tv_nsec;
}

const char *condstr(enum cond_state s)
{
	static const char *strs[] = {
		[COND_OFF]  = "off",
		[COND_FLUX] = "flux",
		[COND_ON]   = "on",
	};

	return strs[s];
}

const char *cond_path(const char *name)
{
	static char file[MAX_ARG_LEN];

	snprintf(file, sizeof(file), COND_PATH "/%s", name);
	return file;
}

enum cond_state cond_get_path(const char *path)
{
	struct stat st, st_reconf;

	if (stat(path, &st))
		return COND_OFF;

	if (stat(COND_RECONF, &st_reconf) ||
	    timespec_newer(&st.st_mtim, &st_reconf.st_mtim))
		return COND_ON;

	return COND_FLUX;
}

enum cond_state cond_get(const char *name)
{
	return cond_get_path(cond_path(name));
}

enum cond_state cond_get_agg(const char *names)
{
	static char conds[MAX_ARG_LEN];

	enum cond_state s = COND_ON;
	char *cond;

	if (!names)
		return COND_ON;

	strlcpy(conds, names, sizeof(conds));
	for (cond = strtok(conds, ","); s && cond; cond = strtok(NULL, ","))
		s = min(s, cond_get(cond));

	return s;
}

int cond_affects(const char *name, const char *names)
{
	static char conds[MAX_ARG_LEN];

	char *cond;

	if (!name || !names)
		return 0;

	strlcpy(conds, names, sizeof(conds));
	for (cond = strtok(conds, ","); cond; cond = strtok(NULL, ","))
		if (!strcmp(cond, name))
			return 1;

	return 0;
}

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
