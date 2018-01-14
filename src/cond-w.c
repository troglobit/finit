/* Condition engine (write)
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

#include <ftw.h>
#include <libgen.h>
#include <lite/lite.h>
#include <stdio.h>

#include "finit.h"
#include "cond.h"
#include "service.h"

static int cond_set_gen(const char *path, unsigned int gen)
{
	FILE *fp;
	int ret;

	fp = fopen(path, "w");
	if (!fp)
		return -1;

	ret = fprintf(fp, "%u\n", gen);
	fclose(fp);

	return (ret > 0) ? 0 : ret;
}

static void cond_bump_reconf(void)
{
	unsigned int rgen;

	/*
	 * If %COND_RECONF does not exist, cond_get_gen() returns 0
	 * meaning that rgen++ is always what we want.
	 */
	rgen = cond_get_gen(COND_RECONF);
	rgen++;

	cond_set_gen(COND_RECONF, rgen);
}

static int cond_checkpath(const char *path)
{
	char buf[MAX_ARG_LEN], *dir;

	strlcpy(buf, path, sizeof(buf));
	dir = dirname(buf);
	if (!dir) {
		_e("Invalid path '%s' for condition", path);
		return 1;
	}

	if (makepath(dir) && errno != EEXIST) {
		_pe("Failed creating dir '%s' for condition '%s'", dir, path);
		return 1;
	}

	return 0;
}

int cond_set_path(const char *path, enum cond_state new)
{
	enum cond_state old;
	unsigned int rgen;

	_d("%s", path);

	rgen = cond_get_gen(COND_RECONF);
	if (!rgen) {
		_e("Unable to read configuration generation (%s)", path);
		return -1;
	}

	old = cond_get_path(path);

	switch (new) {
	case COND_ON:
		if (cond_checkpath(path))
		    return 0;
		cond_set_gen(path, rgen);
		break;

	case COND_OFF:
		if (unlink(path) && errno != ENOENT)
			_pe("Failed removing condition '%s'", path);
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
	svc_t *svc, *iter = NULL;

	_d("%s", name);
	for (svc = svc_iterator(&iter, 1); svc; svc = svc_iterator(&iter, 0)) {
		if (!svc_has_cond(svc) || !cond_affects(name, svc->cond))
			continue;

		_d("%s: match <%s> %s(%s)", name ?: "nil", svc->cond, svc->desc, svc->cmd);
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

void cond_set_oneshot(const char *name)
{
	const char *path = cond_path(name);

	_d("s => %s", name, path);
	if (cond_checkpath(path))
		return;

	symlink(COND_RECONF, path);
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

	cond_bump_reconf();
	cond_update(NULL);
}

static int reassert(const char *fpath, const struct stat *sb, int tflg, struct FTW *ftw)
{
	char *nm;

	if (ftw->level == 0)
		return 1;

	if (tflg != FTW_F)
		return 0;

	nm = (char *)fpath + sizeof(COND_PATH);
	_d("Reasserting %s => %s", fpath, nm);
	cond_set(nm);

	return 0;
}

/*
 * Used only by netlink plugin atm.
 * type: is a one of svc/, net/, etc.
 */
void cond_reassert(const char *type)
{
	_d("%s", type);
	nftw(cond_path(type), reassert, 20, FTW_DEPTH);
}

void cond_init(void)
{
	if (makepath(COND_PATH) && errno != EEXIST) {
		_pe("Failed creating condition base directory '%s'", COND_PATH);
		return;
	}

	cond_bump_reconf();
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
