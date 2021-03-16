/* New client tool, replaces old /dev/initctl API and telinit tool
 *
 * Copyright (c) 2019-2021  Joachim Wiberg <troglobit@gmail.com>
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

#include <err.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <search.h>
#include <inttypes.h>
#include <lite/lite.h>

#include "initctl.h"

#define NONE " "
#define PIPE plain ? "|"  : "│"
#define FORK plain ? "|-" : "├─"
#define END  plain ? "`-" : "└─"

struct cg {
	struct cg *cg_next;

	char      *cg_path;		/* path in /sys/fs/cgroup   */
	uint64_t   cg_prev;		/* cpuacct.usage            */
	float      cg_load;		/* curr - prev / 10000000.0 */
};

struct cg *list;


static char *pid_cmdline(int pid, char *buf, size_t len)
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

static char *pid_comm(int pid, char *buf, size_t len)
{
	FILE *fp;

	fp = fopenf("r", "/proc/%d/comm", pid);
	if (!fp)
		return NULL;

	fgets(buf, len, fp);
	fclose(fp);

	return chomp(buf);
}

static uint64_t cgroup_uint64(char *path, char *file)
{
	uint64_t val = 0;
	char buf[42];
	FILE *fp;

	fp = fopenf("r", "%s/%s", path, file);
	if (!fp)
		return val;

	if (fgets(buf, sizeof(buf), fp))
		val = strtoull(buf, NULL, 10);
	fclose(fp);

	return val;
}

static int cgroup_shares(char *path)
{
	return (int)cgroup_uint64(path, "cpu.shares");
}

static char *cgroup_memuse(char *path)
{
        static char buf[32];
        int gb, mb, kb, b;
	uint64_t num;

	num = cgroup_uint64(path, "memory.usage_in_bytes");

        gb  = num / (1024 * 1024 * 1024);
        num = num % (1024 * 1024 * 1024);
        mb  = num / (1024 * 1024);
        num = num % (1024 * 1024);
        kb  = num / (1024);
        b   = num % (1024);

        if (gb)
                snprintf(buf, sizeof(buf), "%d.%dG", gb, mb / 102);
        else if (mb)
                snprintf(buf, sizeof(buf), "%d.%dM", mb, kb / 102);
        else
                snprintf(buf, sizeof(buf), "%d.%dk", kb, b / 102);

        return buf;
}

static struct cg *append(char *path)
{
	struct cg *cg;
	char fn[256];
	ENTRY item;

	snprintf(fn, sizeof(fn), "%s/cpuacct.usage", path);
	if (access(fn, F_OK)) {
		warn("not a cgroup path with cpuacct controller, %s", path);
		return NULL;
	}

	cg = calloc(1, sizeof(struct cg));
	if (!cg)
		err(1, "failed allocating struct cg");

	cg->cg_path = strdup(path);
	if (list)
		cg->cg_next = list;
	list = cg;

	item.key  = cg->cg_path;
	item.data = cg;
	if (!hsearch(item, ENTER))
		err(1, "failed adding to hash table");

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

static float cgroup_cpuload(char *path)
{
	struct cg *cg;
	char fn[256];
	char buf[64];
	FILE *fp;

	cg = find(path);
	if (!cg)
		return 0.0;

	snprintf(fn, sizeof(fn), "%s/cpuacct.usage", cg->cg_path);
	fp = fopen(fn, "r");
	if (!fp)
		err(1, "Cannot open %s", fn);

	if (fgets(buf, sizeof(buf), fp)) {
		uint64_t curr;

		curr = strtoull(buf, NULL, 10);
		if (cg->cg_prev != 0) {
			uint64_t diff = curr - cg->cg_prev;

			/* this expects 1 sec poll interval */
			cg->cg_load = (float)(diff / 1000000);
			cg->cg_load /= 10.0;
		}
		cg->cg_prev = curr;
	}

	fclose(fp);

	return cg->cg_load;
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

#define CDIM (plain ? "" : "\e[2m")
#define CRST (plain ? "" : "\e[0m")

static int dump_cgroup(char *path, char *pfx, int top)
{
	struct dirent **namelist = NULL;
	struct stat st;
	char buf[512];
	int rc = 0;
	FILE *fp;
	int i, n;
	int num;

	if (top >= screen_rows)
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
		if (top)
			printf(" %6.6s %5.1f  [%s]\n", cgroup_memuse(path),
			       cgroup_cpuload(path), path);
		else
			puts(path);
	}

	n = scandir(path, &namelist, cgroup_filter, alphasort);
	if (n > 0) {
		for (i = 0; i < n; i++) {
			char *nm = namelist[i]->d_name;
			char prefix[80];

			snprintf(buf, sizeof(buf), "%s/%s", path, nm);
			if (top)
				printf(" %6.6s %5.1f  ", cgroup_memuse(buf),
				       cgroup_cpuload(buf));

			if (i + 1 == n) {
				printf("%s%s ", pfx, END);
				snprintf(prefix, sizeof(prefix), "%s     ", pfx);
			} else {
				printf("%s%s ", pfx, FORK);
				snprintf(prefix, sizeof(prefix), "%s%s    ", pfx, PIPE);
			}
			printf("%s/ [cpu.shares: %d]\n", nm, cgroup_shares(buf));

			rc += dump_cgroup(buf, prefix, top + i);

			free(namelist[i]);
		}

		free(namelist);
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
			if (pid_cmdline(pid, buf, sizeof(buf)))
				printf("%s%s%s %s%d %s %s%s\n",
				       top ? "               " : "",
				       pfx, ++i == num ? END : FORK,
				       CDIM, pid, comm, buf, CRST);
			top += i;
			if (top >= screen_rows)
				break;
		}

		return fclose(fp);
	}

	fclose(fp);

	return rc;
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

	return dump_cgroup(arg, NULL, 0);
}

int show_cgtop(char *arg)
{
	char path[512];

	if (!arg)
		arg = FINIT_CGPATH;
	else if (arg[0] != '/') {
		paste(path, sizeof(path), FINIT_CGPATH, arg);
		arg = path;
	}

	if (!hcreate(screen_rows + 25))
		err(1, "failed creating hash table");

	while (1) {
		fputs("\e[2J\e[1;1H", stdout);
		if (heading)
			print_header(" MEMORY  %%CPU  GROUP/SERVICE");
		dump_cgroup(arg, NULL, 1);
		sleep(1);
	}

	return 0;
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
