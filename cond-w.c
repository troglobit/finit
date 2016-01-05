#include <libgen.h>
#include <libite/lite.h>
#include <stdio.h>
#include <utime.h>

#include "finit.h"
#include "cond.h"
#include "service.h"

int cond_set_path(const char *path, enum cond_state new)
{
	static char dir[MAX_ARG_LEN];

	enum cond_state old;

	old = cond_get_path(path);

	switch (new) {
	case COND_ON:
		strlcpy(dir, path, sizeof(dir));
		makepath(dirname(dir));
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

static void cond_update(const char *name)
{
	svc_t *svc;

	_d("%s", name);

	for (svc = svc_iterator(1); svc; svc = svc_iterator(0)) {
		if (svc->type != SVC_TYPE_SERVICE  ||
		    !svc->cond[0] ||
		    (name && !cond_affects(name, svc->cond))) {
			continue;
		}

		_d("%s: match <%s> %s", name, svc->cond, svc->cmd);
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
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
