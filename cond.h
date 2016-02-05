#ifndef FINIT_COND_H_
#define FINIT_COND_H_

#include <paths.h>

#define COND_PATH     _PATH_VARRUN "finit/cond"
#define COND_SVC_PATH COND_PATH "/svc/"
#define COND_RECONF   COND_PATH "/reconf"

typedef enum cond_state {
	COND_OFF = 0,
	COND_FLUX,
	COND_ON
} cond_state_t;

const char     *condstr      (enum cond_state s);
const char     *cond_path    (const char *name);
enum cond_state cond_get_path(const char *path);
enum cond_state cond_get     (const char *name);
enum cond_state cond_get_agg (const char *names);
int             cond_affects (const char *name, const char *names);

int  cond_set_path(const char *path, enum cond_state new);
void cond_set     (const char *name);
void cond_clear   (const char *name);
void cond_reload  (void);

#endif	/* FINIT_COND_H_ */

/**
 * Local Variables:
 *  version-control: t
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
