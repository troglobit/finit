/* New client tool, replaces old /dev/initctl API and telinit tool
 *
 * Copyright (c) 2019-2023  Joachim Wiberg <troglobit@gmail.com>
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

#include <dirent.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <search.h>
#include <inttypes.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif
#include <sys/sysinfo.h>		/* sysinfo() */

#include "cgutil.h"
#include "initctl.h"
#include "log.h"

#define CDIM plain ? "" : "\e[2m"
#define CRST plain ? "" : "\e[0m"

#define NONE " "
#define PIPE plain ? "| " : "│ "
#define FORK plain ? "|-" : "├─"
#define END  plain ? "`-" : "└─"

uint64_t total_ram;			/* From sysinfo() */

struct cg  dummy;			/* empty result "NULL"      */
struct cg *list;

int cgroup_avail(void)
{
	return fismnt(FINIT_CGPATH);
}

char *pid_cmdline(int pid, char *buf, size_t len)
{
	size_t i, sz;
	char *ptr;
	FILE *fp;

	fp = fopenf("r", "/proc/%d/cmdline", pid);
	if (!fp) {
		buf[0] = 0;
		return buf;
	}

	sz = fread(buf, sizeof(buf[0]), len - 1, fp);
	fclose(fp);
	if (!sz)
		return NULL;		/* kernel thread */

	buf[sz] = 0;

	ptr = strchr(buf, 0);
	if (ptr && ptr != buf) {
		ptr++;
		sz -= ptr - buf;
		memmove(buf, ptr, sz + 1);
	}

	for (i = 0; i < sz; i++) {
		if (buf[i] == 0)
			buf[i] = ' ';
	}

	return buf;
}

char *pid_comm(int pid, char *buf, size_t len)
{
	char *ptr = NULL;
	FILE *fp;

	fp = fopenf("r", "/proc/%d/comm", pid);
	if (!fp)
		return NULL;

	if (fgets(buf, len, fp))
		ptr = chomp(buf);
	fclose(fp);

	return ptr;
}

char *pid_cgroup(int pid, char *buf, size_t len)
{
	char *ptr = NULL;
	FILE *fp;

	fp = fopenf("r", "/proc/%d/cgroup", pid);
	if (!fp)
		return NULL;

	if (fgets(buf, len, fp))
		ptr = chomp(buf);
	fclose(fp);

	if (ptr)
		ptr = strchr(buf, '/');

	return ptr;
}

static char *cgroup_val(char *path, char *file, char *buf, size_t len)
{
	char *val = NULL;
	FILE *fp;

	fp = fopenf("r", "%s/%s", path, file);
	if (fp) {
		if (fgets(buf, len, fp)) {
			val = chomp(buf);
			len = strcspn(val, " \t");
			val[len] = 0;
		}

		fclose(fp);
	}

	return val;
}

static uint64_t cgroup_uint64(char *path, char *file)
{
	uint64_t val = 0;
	char buf[42];

	if (cgroup_val(path, file, buf, sizeof(buf)))
		val = strtoull(buf, NULL, 10);

	return val;
}

static char *cgroup_memval(char *path, char *file, char *buf, size_t len)
{
	char data[42];

	if (cgroup_val(path, file, data, sizeof(data))) {
		if (!strcmp(data, "max"))
			strlcpy(buf, data, len);
		else
			memsz(strtoull(data, NULL, 10), buf, len);
	} else
		buf[0] = 0;

	return buf;
}

static uint64_t cgroup_memuse(struct cg *cg)
{
	char buf[42];
	FILE *fp;

	fp = fopenf("r", "%s/memory.stat", cg->cg_path);
	if (fp) {
		cg->cg_rss = 0;
		cg->cg_vmlib = 0;

		while (fgets(buf, sizeof(buf), fp)) {
			chomp(buf);

			if (!strncmp(buf, "anon", 4)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "slab", 4)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "kernel_stack", 12)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "pagetables", 10)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "percpu", 6)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "sock", 4)) {
				cg->cg_rss += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "file", 4)) {
				cg->cg_vmlib += strtoull(&buf[5], NULL, 10);
				continue;
			}
			if (!strncmp(buf, "file_mapped", 11)) {
				cg->cg_vmlib += strtoull(&buf[5], NULL, 10);
				continue;
			}
		}
		fclose(fp);
	}

	cg->cg_memshare = (float)(cg->cg_rss * 100 / total_ram);

	return cg->cg_vmsize = cgroup_uint64(cg->cg_path, "memory.current");
}

uint64_t cgroup_memory(char *group)
{
	char path[256];

	paste(path, sizeof(path), FINIT_CGPATH, group);

	return cgroup_uint64(path, "memory.current");
}

static float cgroup_cpuload(struct cg *cg)
{
	char fn[256];
	char buf[64];
	FILE *fp;

	snprintf(fn, sizeof(fn), "%s/cpu.stat", cg->cg_path);
	fp = fopen(fn, "r");
	if (!fp)
		ERR(72, "Cannot open %s", fn);

	while (fgets(buf, sizeof(buf), fp)) {
		uint64_t curr;

		chomp(buf);
		if (strncmp(buf, "usage_usec", 10))
			continue;

		curr = strtoull(&buf[11], NULL, 10);
		if (cg->cg_prev != 0) {
			uint64_t diff = curr - cg->cg_prev;

			/* this expects 1 sec poll interval */
			cg->cg_load = (float)(diff / 1000000);
			cg->cg_load *= 100.0;
		}
		cg->cg_prev = curr;
		break;
	}

	fclose(fp);

	return cg->cg_load;
}

static struct cg *append(char *path)
{
	struct cg *cg;
	char fn[256];
	ENTRY item;

	snprintf(fn, sizeof(fn), "%s/cpu.stat", path);
	if (access(fn, F_OK)) {
		/* older kernels, 4.19, don't have summary cpu.stat in root */
		if (strcmp(path, FINIT_CGPATH))
			WARN("not a cgroup path with cpu controller, %s", path);
		return NULL;
	}

	cg = calloc(1, sizeof(struct cg));
	if (!cg)
		ERR(71, "failed allocating struct cg");

	cg->cg_path = strdup(path);
	if (list)
		cg->cg_next = list;
	list = cg;

	item.key  = cg->cg_path;
	item.data = cg;
	if (!hsearch(item, ENTER))
		ERR(70, "failed adding to hash table");

	return cg;
}

static struct cg *find(char *path)
{
	ENTRY *ep, item = { path, NULL };

	ep = hsearch(item, FIND);
	if (ep)
		return ep->data;

	return append(path);
}

/* update stats */
struct cg *cg_stats(char *path)
{
	struct cg *cg;

	cg = find(path);
	if (!cg)
		return &dummy;

	cgroup_cpuload(cg);
	cgroup_memuse(cg);

	return cg;
}

/* query config */
struct cg *cg_conf(char *path)
{
	static struct cg cg;

	cgroup_val(path, "memory.min", cg.cg_mem.min, sizeof(cg.cg_mem.min));
	cgroup_memval(path, "memory.max", cg.cg_mem.max, sizeof(cg.cg_mem.max));
	cgroup_val(path, "cpu.weight", cg.cg_cpu.weight, sizeof(cg.cg_cpu.weight));
	cgroup_val(path, "cpu.max",    cg.cg_cpu.max, sizeof(cg.cg_cpu.max));
	cgroup_val(path, "cpuset.cpus.effective", cg.cg_cpu.set, sizeof(cg.cg_cpu.set));
	cg.cg_vmsize = cgroup_uint64(path, "memory.current");

	return &cg;
}

static int cgroup_filter(const struct dirent *entry)
{
	/* Skip current dir ".", and prev dir "..", from list of files */
	if ((1 == strlen(entry->d_name) && entry->d_name[0] == '.') ||
	    (2 == strlen(entry->d_name) && !strcmp(entry->d_name, "..")))
		return 0;

	if (entry->d_name[0] == '.')
		return 0;

	if (entry->d_type != DT_DIR)
		return 0;

	return 1;
}

int cgroup_tree(char *path, char *pfx, int mode, int pos)
{
	struct dirent **namelist = NULL;
	char s[32], r[32], l[32];
	char row[ttcols + 9];		/* + control codes */
	size_t rlen = sizeof(row) - 1;
	size_t rplen = rlen - 9;
	struct stat st;
	struct cg *cg;
	char buf[512];
	int rc = 0;
	FILE *fp;
	int i, n;
	int num;

	if (pos >= ttrows)
		return 0;

	if (-1 == lstat(path, &st))
		return 1;

	if ((st.st_mode & S_IFMT) != S_IFDIR) {
		errno = ENOTDIR;
		return -1;
	}

	fp = fopenf("r", "%s/cgroup.procs", path);
	if (!fp)
		return -1;
	num = 0;
	while (fgets(buf, sizeof(buf), fp))
		num++;

	if (!pfx) {
		pfx = "";
		switch (mode) {
		case 1:
			cg = cg_stats(path);
			snprintf(row, rplen, "\r %6.6s  %6.6s  %6.6s %5.1f %5.1f  %s",
				 memsz(cg->cg_vmsize, s, sizeof(s)),
				 memsz(cg->cg_rss,    r, sizeof(r)),
				 memsz(cg->cg_vmlib,  l, sizeof(l)),
				 cg->cg_memshare, cg->cg_load, path);
			break;
		case 2:
			cg = cg_conf(path);
			snprintf(row, rplen, "\r%6.6s [%-6.6s%6.6s] %6s [%-6.6s%6.6s] %s",
				 memsz(cg->cg_vmsize, s, sizeof(s)),
				 cg->cg_mem.min, cg->cg_mem.max, cg->cg_cpu.set,
				 cg->cg_cpu.weight, cg->cg_cpu.max, path);
			break;
		default:
			strlcpy(row, "\r", rplen);
			strlcat(row, path, rplen);
			break;
		}

		puts(row);
	}

	if (num > 0) {
		rewind(fp);

		i = 0;
		while (fgets(buf, sizeof(buf), fp)) {
			char comm[80] = { 0 };
			pid_t pid;

			pid = atoi(chomp(buf));
			if (pid <= 0)
				continue;

			/* skip kernel threads for now (no cmdline) */
			pid_comm(pid, comm, sizeof(comm));
			if (pid_cmdline(pid, buf, sizeof(buf))) {
				char proc[ttcols];

				switch (mode) {
				case 1:
					snprintf(row, rplen, "\r%37s", " ");
					break;
				case 2:
					snprintf(row, rplen, "\r --.-- [            ]        [            ] ");
					break;
				default:
					strlcpy(row, "\r", rplen);
					break;
				}

				strlcat(row, pfx, rplen);
				strlcat(row, ++i == num ? END : FORK, rlen);

				snprintf(proc, sizeof(proc), " %d %s %s", pid, comm, buf);

				if (plain) {
					strlcat(row, proc, rplen);
				} else {
					int len;

					strlcat(row, CDIM, rlen);
					strlcat(row, proc, rlen);

					len = strlen(row) + strlen(CRST);
					if (len > (int)rlen)
						row[rlen - strlen(CRST)] = 0;

					strlcat(row, CRST, sizeof(row));
				}

				puts(row);
			}

			if (mode == 1) {
				pos += i;
				if (pos >= ttrows)
					break;
			}
		}
	}

	fclose(fp);

	n = scandir(path, &namelist, cgroup_filter, alphasort);
	if (n > 0) {
		for (i = 0; i < n; i++) {
			char *nm = namelist[i]->d_name;
			char prefix[80];

			snprintf(buf, sizeof(buf), "%s/%s", path, nm);
			switch (mode) {
			case 1:
				cg = cg_stats(buf);
				snprintf(row, rplen,
					 "\r %6.6s  %6.6s  %6.6s %5.1f %5.1f  ",
					 memsz(cg->cg_vmsize, s, sizeof(s)),
					 memsz(cg->cg_rss,    r, sizeof(r)),
					 memsz(cg->cg_vmlib,  l, sizeof(l)),
					 cg->cg_memshare, cg->cg_load);
				break;
			case 2:
				cg = cg_conf(buf);
				snprintf(row, rplen, "\r%6.6s [%-6.6s%6.6s] %6.6s [%-6.6s%6.6s] ",
					 memsz(cg->cg_vmsize, s, sizeof(s)),
					 cg->cg_mem.min, cg->cg_mem.max,
					 cg->cg_cpu.set, cg->cg_cpu.weight, cg->cg_cpu.max);
				break;
			default:
				strlcpy(row, "\r", rplen);
				break;
			}

			strlcat(row, pfx, rplen);
			if (i + 1 == n) {
				strlcat(row, END, rplen);
				snprintf(prefix, sizeof(prefix), "%s   ", pfx);
			} else {
				strlcat(row, FORK, rplen);
				snprintf(prefix, sizeof(prefix), "%s%s  ", pfx, PIPE);
			}
			strlcat(row, " ", rplen);

			strlcat(row, nm,   rplen);
			strlcat(row, "/ ", rplen);

			puts(row);

			rc += cgroup_tree(buf, prefix, mode, pos + i);

			free(namelist[i]);
		}

		free(namelist);
	}

	return rc;
}

int show_cgps(char *arg)
{
	char path[512];

	if (!arg)
		arg = FINIT_CGPATH;
	else if (arg[0] != '/') {
		paste(path, sizeof(path), FINIT_CGPATH, arg);
		arg = path;
	}

	return cgroup_tree(arg, NULL, 0, 0);
}

static void cgtop(uev_t *w, void *arg, int events)
{
	(void)w;
	(void)events;

	fputs("\e[2J\e[1;1H", stdout);
	if (heading)
		print_header(" VmSIZE     RSS   VmLIB  %%MEM  %%CPU  GROUP");
	cgroup_tree(arg, NULL, 1, 0);
}

static void cleanup(void)
{
	ttcooked();
	showcursor();
	puts("");
}

static void leave(uev_t *w, void *arg, int events)
{
	(void)arg;
	(void)events;

	uev_exit(w->ctx);
}

static void key(uev_t *w, void *arg, int events)
{
	char ch;

	(void)arg;
	(void)events;

	if (read(w->fd, &ch, sizeof(ch)) != -1) {
		switch (ch) {
		case 'q':
			uev_exit(w->ctx);
			break;

		default:
			dbg("Got char 0x%02x", ch);
			break;
		}
	}
}

int show_cgtop(char *arg)
{
	struct sysinfo si = { 0 };
        uev_t timer, input, sigint, sigterm, sigquit;
	char path[512];
        uev_ctx_t ctx;

	if (!arg)
		arg = FINIT_CGPATH;
	else if (arg[0] != '/') {
		paste(path, sizeof(path), FINIT_CGPATH, arg);
		arg = path;
	}

	if (!hcreate(ttrows + 25))
		ERR(70, "failed creating hash table");

	sysinfo(&si);
	total_ram = si.totalram * si.mem_unit;

        uev_init(&ctx);
        uev_timer_init(&ctx, &timer, cgtop, arg, 1, ionce ? 0 : 1000);

	if (!ionce && !plain) {
		int flags;

		atexit(cleanup);
		ttraw();
		hidecursor();

		flags = fcntl(STDIN_FILENO, F_GETFL);
		if (flags != -1)
			(void)fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
		(void)uev_io_init(&ctx, &input, key, NULL, STDIN_FILENO, UEV_READ);

		(void)uev_signal_init(&ctx, &sigint, leave, NULL, SIGINT);
		(void)uev_signal_init(&ctx, &sigterm, leave, NULL, SIGTERM);
		(void)uev_signal_init(&ctx, &sigquit, leave, NULL, SIGQUIT);
	}

	return uev_run(&ctx, 0);
}

int show_cgroup(char *arg)
{
	char path[512];

	if (!arg)
		arg = FINIT_CGPATH;
	else if (arg[0] != '/') {
		paste(path, sizeof(path), FINIT_CGPATH, arg);
		arg = path;
	}

	/* memory.current memory.min memory.max cpuset.cpus cpu.weight cpu.max */
	if (heading)
		print_header("   MEM [MIN      MAX]    CPU [WEIGHT   MAX] GROUP");

	return cgroup_tree(arg, NULL, 2, 0);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
