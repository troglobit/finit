/* Condition engine (read)
 *
 * Copyright (c) 2015-2017  Tobias Waldekranz <tobias@waldekranz.com>
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

#include <lite/lite.h>
#include <stdio.h>

#include "finit.h"
#include "cond.h"
#include "pid.h"
#include "service.h"


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
	char tmp[MAX_ARG_LEN];
	static char file[MAX_ARG_LEN];

	snprintf(tmp, sizeof(tmp), COND_PATH "/%s", name);

	return pid_runpath(tmp, file, sizeof(file));
}

unsigned int cond_get_gen(const char *file)
{
	char *ptr, path[256];
	unsigned int gen;
	FILE *fp;
	int ret;

	/* /var/run --> /run symlink may not exist (yet) */
	ptr = pid_runpath(file, path, sizeof(path));

	fp = fopen(ptr, "r");
	if (!fp)
		return 0;

	ret = fscanf(fp, "%u", &gen);
	fclose(fp);

	return (ret == 1) ? gen : 0;
}

enum cond_state cond_get_path(const char *path)
{
	int cgen, rgen;

	rgen = cond_get_gen(COND_RECONF);
	if (!rgen)
		return COND_OFF;

	cgen = cond_get_gen(path);
	if (!cgen)
		return COND_OFF;

	return (cgen == rgen) ? COND_ON : COND_FLUX;
}

enum cond_state cond_get(const char *name)
{
	return cond_get_path(cond_path(name));
}

enum cond_state cond_get_agg(const char *names)
{
	char *cond;
	enum cond_state s = COND_ON;
	static char conds[MAX_ARG_LEN];

	if (!names)
		return COND_ON;

	strlcpy(conds, names, sizeof(conds));
	for (cond = strtok(conds, ","); s && cond; cond = strtok(NULL, ","))
		s = min(s, cond_get(cond));

	return s;
}

int cond_affects(const char *name, const char *names)
{
	char *cond;
	static char conds[MAX_ARG_LEN];

	if (!name || !names)
		return 0;

	strlcpy(conds, names, sizeof(conds));
	for (cond = strtok(conds, ","); cond; cond = strtok(NULL, ",")) {
		if (!strcmp(cond, name))
			return 1;
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
