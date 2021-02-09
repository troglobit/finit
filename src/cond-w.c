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
#include "pid.h"
#include "service.h"

/*
 * The service condition name is constructed from the 'pid/' prefix,
 * the dirname of the svc->cmd, e.g., '/usr/bin/teamd' => 'usr/bin', and
 * the service's pid:filename, including an optional subdirectory,
 * without the .pid extension.
 *
 * The following example uses the team (aggregate) service:
 *
 *    service pid:!/run/teamd/a1.pid /usr/bin/teamd -f /etc/teamd/a1.conf
 *
 * => 'pid/' + 'usr/bin' + 'teamd/a1' => pid/usr/bin/teamd/a1
 *
 * The next example uses the dbus-daemon:
 *
 *    service pid:!/run/dbus/pid /usr/bin/dbus-daemon
 *
 * => 'pid/' + 'usr/bin' + 'dbus' => pid/usr/bin/dbus
 *
 * The last example uses lxc-start to start container foo:
 *
 *    service pid:!/run/lxc/foo.pid lxc-start -n foo -F -p /run/lxc/foo.pid -- Container foo
 *
 * => 'pid/' + '' + 'lxc/foo' => pid/lxc/foo
 *
 * Note: previously the condition ws called 'svc/..', the conf.c parser
 *       automatically translates to the 'pid/..' prefix and warns.
 */
char *mkcond(svc_t *svc, char *buf, size_t len)
{
	char path[256];
	char *pidfile;
	char *ptr, *nm;

	strlcpy(path, svc->cmd, sizeof(path));
	ptr = rindex(path, '/');
	if (ptr)
		*ptr++ = 0;
	else
		path[0] = 0;

	/* Figure out default name used when registering service */
	if (ptr)
		nm = ptr;
	else
		nm = svc->cmd;

	pidfile = pid_file(svc);
	ptr = strstr(pidfile, "run");
	if (ptr)
		ptr += 3;
	else
		ptr = rindex(pidfile, '/');

	/* Custom name:foo declaration found => pid/foo instead of /pid/bin/path/pidfile-.pid */
	if (strcmp(nm, svc->name)) {
		snprintf(buf, len, "pid/%s", svc->name);
		_d("Composed condition from svc->name %s => %s", svc->name, buf);
	} else {
		snprintf(buf, len, "pid%s%s%s", path[0] != 0 && path[0] != '/' ? "/" : "", path, ptr);
		_d("Composed condition from cmd %s (path %s) and pidfile %s => %s", svc->cmd, path, ptr, buf);
	}

	/* Case: /var/run/dbus/pid */
	ptr = strstr(buf, "/pid");
	if (ptr && !strcmp(ptr, "/pid"))
		*ptr = 0;

	/* Case /var/run/teamd/a1.pid */
	ptr = strstr(buf, ".pid");
	if (ptr && !strcmp(ptr, ".pid"))
		*ptr = 0;

	/* Always append /ID if service is declared with :ID */
	if (svc->id[0]) {
		strlcat(buf, "/", len);
		strlcat(buf, svc->id, len);
	}

	_d("Creating condition => %s", buf);

	return buf;
}

static int cond_set_gen(const char *file, unsigned int gen)
{
	char *ptr, path[256];
	FILE *fp;
	int ret;

	/* /var/run --> /run symlink may not exist (yet) */
	ptr = pid_runpath(file, path, sizeof(path));

	fp = fopen(ptr, "w");
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
	case SVC_TYPE_SYSV:
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
	if (string_compare(name, "nop"))
		return;

	if (!cond_set_path(cond_path(name), COND_ON))
		return;

	cond_update(name);
}

void cond_set_oneshot(const char *name)
{
	const char *path;

	if (string_compare(name, "nop"))
		return;

	path = cond_path(name);
	_d("%s => %s", name, path);

	if (cond_checkpath(path))
		return;

	symlink(COND_RECONF, path);
	cond_update(name);
}

void cond_clear(const char *name)
{
	_d("%s", name);
	if (string_compare(name, "nop"))
		return;

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

	nm = strstr((char *)fpath, COND_DIR);
	if (!nm) {
		_e("Incorrect condtion path %s, cannot reassert", fpath);
		return 1;
	}

	nm += sizeof(COND_DIR);
	_d("Reasserting %s => %s", fpath, nm);
	cond_set(nm);

	return 0;
}

/*
 * Used only by netlink plugin atm.
 * type: is a one of pid/, net/, etc.
 */
void cond_reassert(const char *type)
{
	_d("%s", type);
	nftw(cond_path(type), reassert, 20, FTW_DEPTH);
}

void cond_init(void)
{
	char path[MAX_ARG_LEN];

	if (makepath(pid_runpath(COND_PATH, path, sizeof(path))) && errno != EEXIST) {
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
