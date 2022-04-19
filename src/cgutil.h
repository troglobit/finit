/* Cgroup utility functions for initctl
 *
 * Copyright (c) 2019-2022  Joachim Wiberg <troglobit@gmail.com>
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

#ifndef FINIT_CGUTIL_H_
#define FINIT_CGUTIL_H_

#include <stdint.h>
#include <stdlib.h>

struct cg {
	struct cg *cg_next;

	/* stats */
	char      *cg_path;		/* path in /sys/fs/cgroup   */
	uint64_t   cg_prev;		/* cpuacct.usage            */
	uint64_t   cg_rss;		/* memory.stat              */
	uint64_t   cg_vmlib;		/* memory.stat              */
	uint64_t   cg_vmsize;		/* memory.current           */
	float      cg_memshare;		/* cg_rss / total_ram * 100 */
	float      cg_load;		/* curr - prev / 10000000.0 */

	/* config */
	struct {
		char min[32];
		char max[32];
	} cg_mem;

	struct {
		char set[16];
		char weight[32];
		char max[32];
	} cg_cpu;
};

int cgroup_avail(void);

char *pid_cmdline (int pid, char *buf, size_t len);
char *pid_comm    (int pid, char *buf, size_t len);
char *pid_cgroup  (int pid, char *buf, size_t len);

uint64_t cgroup_memory(char *group);

struct cg *cg_stats(char *path);
struct cg *cg_conf (char *path);

int   cgroup_tree  (char *path, char *pfx, int mode, int pos);

int   show_cgroup (char *arg);
int   show_cgtop  (char *arg);
int   show_cgps   (char *arg);

#endif /* FINIT_CGUTIL_H_ */

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
