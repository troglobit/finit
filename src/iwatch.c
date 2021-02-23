#include <errno.h>

#include "finit.h"
#include "iwatch.h"
#include "log.h"


int iwatch_init(struct iwatch *iw)
{
	if (!iw) {
		errno = EINVAL;
		return -1;
	}

	TAILQ_INIT(&iw->iwp_list);

	iw->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (iw->fd < 0) {
		_pe("inotify_init()");
		return -1;
	}

	return iw->fd;
}

void iwatch_exit(struct iwatch *iw)
{
	struct iwatch_path *iwp, *tmp;

	TAILQ_FOREACH_SAFE(iwp, &iw->iwp_list, link, tmp) {
		TAILQ_REMOVE(&iw->iwp_list, iwp, link);
		inotify_rm_watch(iw->fd, iwp->wd);
		free(iwp->path);
		free(iwp);
	}

	close(iw->fd);
}

int iwatch_add(struct iwatch *iw, char *file, uint32_t mask)
{
	struct iwatch_path *iwp;
	char *path;
	int wd;

	_d("Adding new watcher for path %s", file);

	path = strdup(file);
	if (!path) {
		_pe("Out of memory");
		return -1;
	}

	wd = inotify_add_watch(iw->fd, path, IWATCH_MASK | mask);
	if (wd < 0) {
		_pe("inotify_add_watch(%s)", path);
		free(path);
		return -1;
	}

	iwp = malloc(sizeof(struct iwatch_path));
	if (!iwp) {
		_pe("Failed allocating new `struct iwatch_path`");
		inotify_rm_watch(iw->fd, wd);
		free(path);
		return -1;
	}

	iwp->path = path;
	iwp->wd = wd;
	TAILQ_INSERT_HEAD(&iw->iwp_list, iwp, link);

	return 0;
}

int iwatch_del(struct iwatch *iw, struct iwatch_path *iwp)
{
	_d("Removing watcher for removed path %s", iwp->path);

	TAILQ_REMOVE(&iw->iwp_list, iwp, link);
	inotify_rm_watch(iw->fd, iwp->wd);
	free(iwp->path);
	free(iwp);

	return 0;
}

struct iwatch_path *iwatch_find_by_wd(struct iwatch *iw, int wd)
{
	struct iwatch_path *iwp;

	TAILQ_FOREACH(iwp, &iw->iwp_list, link) {
		if (iwp->wd == wd)
			return iwp;
	}

	return NULL;
}

struct iwatch_path *iwatch_find_by_path(struct iwatch *iw, const char *path)
{
	struct iwatch_path *iwp;

	TAILQ_FOREACH(iwp, &iw->iwp_list, link) {
		if (!strcmp(iwp->path, path))
			return iwp;
	}

	return NULL;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */

