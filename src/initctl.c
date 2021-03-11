/* New client tool, replaces old /dev/initctl API and telinit tool
 *
 * Copyright (c) 2015-2021  Joachim Wiberg <troglobit@gmail.com>
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
#include <ftw.h>
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <arpa/inet.h>
#include <lite/lite.h>

#include "client.h"
#include "cond.h"
#include "serv.h"
#include "service.h"
#include "util.h"

#define NONE " "
#define PIPE plain ? "|"  : "│"
#define FORK plain ? "|-" : "├─"
#define END  plain ? "`-" : "└─"

struct cmd {
	char        *cmd;
	struct cmd  *ctx;
	int        (*cb)(char *arg);
};

int icreate  = 0;
int iforce   = 0;
int heading  = 1;
int verbose  = 0;
int plain    = 0;
int runlevel = 0;
int iw, pw;

extern int reboot_main(int argc, char *argv[]);


/* figure ut width of IDENT and PID columns */
static void col_widths(void)
{
	char ident[MAX_IDENT_LEN];
	char pid[10];
	svc_t *svc;

	iw = 0;
	pw = 0;

	for (svc = client_svc_iterator(1); svc; svc = client_svc_iterator(0)) {
		int w, p;

		svc_ident(svc, ident, sizeof(ident));
		w = (int)strlen(ident);
		if (w > iw)
			iw = w;

		p = snprintf(pid, sizeof(pid), "%d", svc->pid);
		if (p > pw)
			pw = p;
	}

	/* adjust for min col width */
	if (iw < 5)
		iw = 5;
	if (pw < 3)
		pw = 3;
}

void print_header(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (plain) {
		int len;

		vfprintf(stdout, fmt, ap);
		fputs("\n", stdout);
		for (len = 0; len < screen_cols; len++)
			fputc('=', stdout);
		fputs("\n", stdout);
	} else {
		char buf[screen_cols];

		vsnprintf(buf, sizeof(buf), fmt, ap);
		printheader(stdout, buf, 0);
	}

        va_end(ap);
}

static int runlevel_get(int *prevlevel)
{
	int result;
	struct init_request rq;

	memset(&rq, 0, sizeof(rq));
	rq.cmd = INIT_CMD_GET_RUNLEVEL;
	rq.magic = INIT_MAGIC;

	result = client_send(&rq, sizeof(rq));
	if (!result) {
		result = rq.runlevel;
		if (prevlevel)
			*prevlevel = rq.sleeptime;
	}

	return result;
}

static int toggle_debug(char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_DEBUG,
	};

	return client_send(&rq, sizeof(rq));
}

static int do_log(char *svc)
{
	char cmd[128];
	char *logfile = "/var/log/syslog";

	if (!svc || !svc[0])
		svc = "finit";

	if (!fexist(logfile))
		logfile = "/var/log/messages";

	snprintf(cmd, sizeof(cmd), "cat %s | grep %s | tail -10", logfile, svc);

	return system(cmd);
}

static int do_runlevel(char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RUNLVL,
	};

	if (!arg) {
		int prevlevel = 0;
		int runlevel;
		char prev;

		runlevel = runlevel_get(&prevlevel);
		if (255 == runlevel) {
			printf("unknown\n");
			return 0;
		}

		prev = prevlevel + '0';
		if (prev <= '0' || prev > '9')
			prev = 'N';

		printf("%c %d\n", prev , runlevel);
		return 0;
	}

	/* set runlevel */
	rq.runlevel = (int)arg[0];

	return client_send(&rq, sizeof(rq));
}

static int do_svc(int cmd, char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = cmd,
	};

	if (arg)
		strlcpy(rq.data, arg, sizeof(rq.data));

	return client_send(&rq, sizeof(rq));
}

static int do_reload (char *arg) { return do_svc(INIT_CMD_RELOAD,      arg); }

/*
 * This is a wrapper for do_svc() that adds a simple sanity check of
 * the service(s) provided as argument.  If a service does not exist
 * we make sure to return an error code.
 */
static int do_startstop(int cmd, char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd   = INIT_CMD_SVC_QUERY
	};

	if (!arg || !arg[0])
		errx(1, "missing argument to %s", (cmd == INIT_CMD_START_SVC
						   ? "start"
						   : (cmd == INIT_CMD_STOP_SVC
						      ? "stop"
						      : "restart")));

	strlcpy(rq.data, arg, sizeof(rq.data));
	if (client_send(&rq, sizeof(rq))) {
		fprintf(stderr, "No such task or service(s): %s\n\n", arg);
		fprintf(stderr, "Usage: initctl %s <NAME>[:ID]\n",
			cmd == INIT_CMD_START_SVC ? "start" :
			(cmd == INIT_CMD_STOP_SVC ? "stop"  : "restart"));
		return 1;
	}

	return do_svc(cmd, arg);
}

static int do_start  (char *arg) { return do_startstop(INIT_CMD_START_SVC,   arg); }
static int do_stop   (char *arg) { return do_startstop(INIT_CMD_STOP_SVC,    arg); }
static int do_restart(char *arg) { return do_startstop(INIT_CMD_RESTART_SVC, arg); }

static int dump_one_cond(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
	const char *cond, *asserted;
	char *nm = "init";
	pid_t pid = 1;

	if (tflag != FTW_F)
		return 0;

	if (!strcmp(fpath, _PATH_RECONF))
		return 0;

	asserted = condstr(cond_get_path(fpath));
	cond = &fpath[strlen(_PATH_COND)];
	if (strncmp("pid/", cond, 4) == 0) {
		svc_t *svc;

		svc= client_svc_find_by_cond(cond);
		if (!svc) {
			nm  = "unknown";
			pid = 0;
		} else {
			nm  = svc_ident(svc, NULL, 0);
			pid = svc->pid;
		}
	} else if (strncmp("usr/", cond, 4) == 0) {
		nm  = "static";
		pid = 0;
	} else if (strncmp("hook/", cond, 4) == 0) {
		nm  = "static";
		pid = 1;
	}

	printf("%-*d  %-*s  %-6s  %s\n", pw, pid, iw, nm, asserted, cond);

	return 0;
}

static int do_cond_dump(char *arg)
{
	col_widths();
	if (heading) {
		char title[80];

		snprintf(title, sizeof(title), "%-*s  %-*s  %-6s  %s", pw, "PID",
			 iw, "IDENT", "STATUS", "CONDITION");
		print_header(title);
	}

	if (nftw(_PATH_COND, dump_one_cond, 20, 0) == -1) {
		warnx("Failed parsing %s", _PATH_COND);
		return 1;
	}

	return 0;
}

static int do_cond_act(char *arg, int creat)
{
	char oneshot[256];
	size_t off;

	if (arg && strncmp(arg, COND_USR, strlen(COND_USR)) == 0)
		arg += strlen(COND_USR);

	if (!arg || !arg[0])
		errx(1, "Invalid condition (empty)");
	if (strchr(arg, '/'))
		errx(1, "Invalid condition (slashes)");
	if (strchr(arg, '.'))
		errx(1, "Invalid condition (periods)");

	snprintf(oneshot, sizeof(oneshot), _PATH_CONDUSR "%s", arg);
	off = strlen(_PATH_COND);

	if (creat) {
		if (symlink(_PATH_RECONF, oneshot) && errno != EEXIST)
			err(1, "Failed asserting condition <%s>", &oneshot[off]);
	} else {
		if (erase(oneshot))
			err(1, "Failed deasserting condition <%s>", &oneshot[off]);
	}

	return 0;
}

static int do_cond_set(char *arg) { return do_cond_act(arg, 1); }
static int do_cond_clr(char *arg) { return do_cond_act(arg, 0); }

static void show_cond_one(const char *_conds)
{
	static char conds[MAX_COND_LEN];
	char *cond;

	strlcpy(conds, _conds, sizeof(conds));

	putchar('<');

	for (cond = strtok(conds, ","); cond; cond = strtok(NULL, ",")) {
		if (cond != conds)
			putchar(',');

		switch (cond_get(cond)) {
		case COND_ON:
			printf("+%s", cond);
			break;

		case COND_FLUX:
			printf("\e[1m~%s\e[0m", cond);
			break;

		case COND_OFF:
			printf("\e[1m-%s\e[0m", cond);
			break;
		}
	}

	putchar('>');
}

static int do_cond_show(char *arg)
{
	char ident[MAX_IDENT_LEN];
	enum cond_state cond;
	svc_t *svc;

	col_widths();
	if (heading) {
		char title[80];

		snprintf(title, sizeof(title), "%-*s  %-*s  %-6s  %s", pw, "PID",
			 iw, "IDENT", "STATUS", "CONDITION (+ ON, ~ FLUX, - OFF)");
		print_header(title);
	}

	for (svc = client_svc_iterator(1); svc; svc = client_svc_iterator(0)) {
		if (!svc->cond[0])
			continue;

		cond = cond_get_agg(svc->cond);

		svc_ident(svc, ident, sizeof(ident));
		printf("%-*d  %-*s  ", pw, svc->pid, iw, ident);

		if (cond == COND_ON)
			printf("%-6.6s  ", condstr(cond));
		else
			printf("\e[1m%-6.6s\e[0m  ", condstr(cond));

		show_cond_one(svc->cond);
		puts("");
	}

	return 0;
}

static int do_cmd(int cmd)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = cmd,
	};

	if (client_send(&rq, sizeof(rq))) {
		if (rq.cmd == INIT_CMD_NACK)
			puts(rq.data);

		return 1;
	}

	/* Wait here for systemd to shutdown/reboot */
	sleep(5);

	return 0;
}

int do_reboot  (char *arg) { return do_cmd(INIT_CMD_REBOOT);   }
int do_halt    (char *arg) { return do_cmd(INIT_CMD_HALT);     }
int do_poweroff(char *arg) { return do_cmd(INIT_CMD_POWEROFF); }
int do_suspend (char *arg) { return do_cmd(INIT_CMD_SUSPEND);  }

int utmp_show(char *file)
{
	static int once = 0;
	struct tm *sectm;
	struct utmp *ut;
	time_t sec;

	int pid;
	char id[sizeof(ut->ut_id) + 1], user[sizeof(ut->ut_user) + 1], when[80];

	if (heading) {
		print_header("%s%s ", once ? "\n" : "", file);
		once++;
	}
	utmpname(file);

	setutent();
	while ((ut = getutent())) {
		char addr[64];

		memset(id, 0, sizeof(id));
		strlcpy(id, ut->ut_id, sizeof(id));

		memset(user, 0, sizeof(user));
		strlcpy(user, ut->ut_user, sizeof(user));

		sec = ut->ut_tv.tv_sec;
		sectm = localtime(&sec);
		strftime(when, sizeof(when), "%F %T", sectm);

		pid = ut->ut_pid;
		if (ut->ut_addr_v6[1] || ut->ut_addr_v6[2] || ut->ut_addr_v6[3])
			inet_ntop(AF_INET6, ut->ut_addr_v6, addr, sizeof(addr));
		else
			inet_ntop(AF_INET, &ut->ut_addr, addr, sizeof(addr));

		printf("[%d] [%05d] [%-4.4s] [%-8.8s] [%-12.12s] [%-20.20s] [%-15.15s] [%-19.19s]\n",
		       ut->ut_type, pid, id, user, ut->ut_line, ut->ut_host, addr, when);
	}
	endutent();

	return 0;
}

static int do_utmp(char *file)
{
	if (!has_utmp())
		return 1;

	if (fexist(file))
		return utmp_show(file);

	return  utmp_show(_PATH_WTMP) ||
		utmp_show(_PATH_UTMP);
}

static int show_version(char *arg)
{
	puts("v" PACKAGE_VERSION);
	return 0;
}

/**
 * runlevel_string - Convert a bit encoded runlevel to .conf syntax
 * @levels: Bit encoded runlevels
 *
 * Returns:
 * Pointer to string on the form [2345], may be up to 12 chars long,
 * plus escape sequences for highlighting current runlevel.
 */
char *runlevel_string(int runlevel, int levels)
{
	int i, pos = 1;
	static char lvl[21];

	memset(lvl, 0, sizeof(lvl));
	lvl[0] = '[';

	for (i = 0; i < 10; i++) {
		if (ISSET(levels, i)) {
			if (!plain && runlevel == i)
				pos = strlcat(lvl, "\e[1m", sizeof(lvl));

			if (i == 0)
				lvl[pos++] = 'S';
			else
				lvl[pos++] = '0' + i;

			if (!plain && runlevel == i)
				pos = strlcat(lvl, "\e[0m", sizeof(lvl));
		} else {
			lvl[pos++] = '-';
		}
	}

	lvl[pos++] = ']';
	lvl[pos]   = 0;

	return lvl;
}

static int missing(svc_t *svc)
{
	if (svc->state == SVC_HALTED_STATE && svc_is_missing(svc))
		return 1;

	return 0;
}

static char *svc_command(svc_t *svc, char *buf, size_t len)
{
	int bold = missing(svc);

	if (plain || whichp(svc->cmd))
		bold = 0;

	strlcpy(buf, bold ? "\e[1m" : "", len);
	strlcat(buf, svc->cmd, len);

	for (int i = 1; i < MAX_NUM_SVC_ARGS; i++) {
		if (!svc->args[i][0])
			break;

		strlcat(buf, " ", len);
		strlcat(buf, svc->args[i], len);
	}

	strlcat(buf, bold ? "\e[0m" : "", len);

	return buf;
}

static char *svc_environ(svc_t *svc, char *buf, size_t len)
{
	int bold = missing(svc);

	if (plain || svc_checkenv(svc))
		bold = 0;

	strlcpy(buf, bold ? "\e[1m" : "", len);
	strlcat(buf, svc->env, len);
	strlcat(buf, bold ? "\e[0m" : "", len);

	return buf;
}

static char *status(svc_t *svc)
{
	static char buf[25];
	int bold = !plain;
	char *s;

	s = svc_status(svc);
	if (svc->state != SVC_HALTED_STATE)
		bold = 0;

	snprintf(buf, sizeof(buf), "%s%-8.8s%s",
		 bold ? "\e[1m" : "", s, bold ? "\e[0m" : "");

	return buf;
}

static int show_status(char *arg)
{
	char ident[MAX_IDENT_LEN];
	char buf[512];
	svc_t *svc;

	runlevel = runlevel_get(NULL);

	if (arg && arg[0]) {
		long now = jiffies();
		char uptm[42] = "N/A";

		svc = client_svc_find(arg);
		if (!svc)
			return 1;

		printf("Status      : %s\n", status(svc));
		printf("Identity    : %s\n", svc_ident(svc, ident, sizeof(ident)));
		printf("Description : %s\n", svc->desc);
		printf("Origin      : %s\n", svc->file[0] ? svc->file : "built-in");
		printf("Environment : %s\n", svc_environ(svc, buf, sizeof(buf)));
		printf("Condition(s): %s\n", svc->cond);
		printf("Command     : %s\n", svc_command(svc, buf, sizeof(buf)));
		printf("PID         : %d\n", svc->pid);
		printf("User        : %s\n", svc->username);
		printf("Group       : %s\n", svc->group);
		printf("Uptime      : %s\n", svc->pid ? uptime(now - svc->start_time, uptm, sizeof(uptm)) : uptm);
		printf("Runlevels   : %s\n", runlevel_string(runlevel, svc->runlevels));
		printf("\n");

		return do_log(svc->cmd);
	}

	col_widths();
	if (heading) {
		char title[80];

		snprintf(title, sizeof(title), "%-*s  %-*s  %-8s %-12s ",
			 pw, "PID", iw, "IDENT", "STATUS", "RUNLEVELS");
		if (!verbose)
			strlcat(title, "DESCRIPTION", sizeof(title)); 
		else
			strlcat(title, "COMMAND", sizeof(title)); 

		print_header(title);
	}

	for (svc = client_svc_iterator(1); svc; svc = client_svc_iterator(0)) {
		char *lvls;

		printf("%-*d  ", pw, svc->pid);

		svc_ident(svc, ident, sizeof(ident));
		printf("%-*s  %s ", iw, ident, status(svc));

		lvls = runlevel_string(runlevel, svc->runlevels);
		if (strchr(lvls, '\e'))
			printf("%-20.20s ", lvls);
		else
			printf("%-12.12s ", lvls);

		if (!verbose)
			puts(svc->desc);
		else
			puts(svc_command(svc, buf, sizeof(buf)));
	}

	return 0;
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

static int dump_cgroup(char *path, char *pfx)
{
	struct dirent **namelist = NULL;
	struct stat st;
	char buf[512];
	int rc = 0;
	FILE *fp;
	int i, n;
	int num;

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
		puts(path);
	}

	n = scandir(path, &namelist, cgroup_filter, alphasort);
	if (n > 0) {
		for (i = 0; i < n; i++) {
			char *nm = namelist[i]->d_name;
			char dir[80];

			snprintf(buf, sizeof(buf), "%s/%s", path, nm);

			if (i + 1 == n) {
				printf("%s%s ", pfx, END);
				snprintf(dir, sizeof(dir), "%s     ", pfx);
			} else {
				printf("%s%s ", pfx, FORK);
				snprintf(dir, sizeof(dir), "%s%s    ", pfx, PIPE);
			}
			puts(nm);

			rc += dump_cgroup(buf, dir);

			free(namelist[i]);
		}

		free(namelist);
	}

	if (num > 0) {
		rewind(fp);

		i = 0;
		while (fgets(buf, sizeof(buf), fp)) {
			FILE *cfp;
			pid_t pid;

			pid = atoi(chomp(buf));
			cfp = fopenf("r", "/proc/%d/cmdline", pid);
			if (!cfp)
				continue;

			if (fgets(buf, sizeof(buf), cfp))
				printf("%s%s %d %s\n", pfx, ++i == num ? END : FORK, pid, buf);
			fclose(cfp);
		}

		return fclose(fp);
	}

	fclose(fp);

	return rc;
}

static int show_cgroup(char *arg)
{
	char path[512];

	if (!arg)
		arg = FINIT_CGPATH;
	else if (arg[0] != '/') {
		paste(path, sizeof(path), FINIT_CGPATH, arg);
		arg = path;
	}

	return dump_cgroup(arg, NULL);
}

static int transform(char *nm)
{
	char *names[] = {
		"reboot", "shutdown", "poweroff", "halt", "suspend",
		NULL
	};
	size_t i;

	for (i = 0; names[i]; i++) {
		if (!strcmp(nm, names[i]))
			return 1;
	}

	return 0;
}

static int has_conf(char *path, size_t len, char *name)
{
	paste(path, len, FINIT_RCSD, name);
	if (!fisdir(path)) {
		strlcpy(path, FINIT_RCSD, len);
		return 0;
	}

	return 1;
}

static int usage(int rc)
{
	int has_rcsd = fisdir(FINIT_RCSD);
	int has_ena;
	char avail[256];
	char ena[256];

	has_conf(avail, sizeof(avail), "available");
	has_ena = has_conf(ena, sizeof(ena), "enabled");

	fprintf(stderr,
		"Usage: %s [OPTIONS] [COMMAND]\n"
		"\n"
		"Options:\n"
		"  -b, --batch               Batch mode, no screen size probing\n"
		"  -c, --create              Create missing paths (and files) as needed\n"
		"  -f, --force               Ignore missing files and arguments, never prompt\n"
		"  -p, --plain               Use plain table headings, no ctrl chars\n"
		"  -t, --no-heading          Skip table headings\n"
		"  -v, --verbose             Verbose output\n"
		"  -h, --help                This help text\n"
		"\n"
		"Commands:\n"
		"  debug                     Toggle Finit (daemon) debug\n"
		"  help                      This help text\n"
		"  version                   Show Finit version\n"
		"\n", prognm);

	if (has_rcsd)
		fprintf(stderr,
			"  ls | list                 List all .conf in " FINIT_RCSD "\n"
			"  create   <CONF>           Create   .conf in %s\n"
			"  delete   <CONF>           Delete   .conf in %s\n"
			"  show     <CONF>           Show     .conf in %s\n"
			"  edit     <CONF>           Edit     .conf in %s\n"
			"  touch    <CONF>           Change   .conf in %s\n",
			avail, avail, avail, avail, avail);
	else
		fprintf(stderr,
			"  show                      Show     %s\n", FINIT_CONF);

	if (has_ena)
		fprintf(stderr,
			"  enable   <CONF>           Enable   .conf in %s\n", avail);
	if (has_ena)
		fprintf(stderr,
			"  disable  <CONF>           Disable  .conf in %s\n", ena);
	if (has_rcsd)
		fprintf(stderr,
			"  reload                    Reload  *.conf in " FINIT_RCSD " (activate changes)\n");
	else
		fprintf(stderr,
			"  reload                    Reload " FINIT_CONF " (activate changes)\n");

	fprintf(stderr,
//		"  reload   <NAME>[:ID]      Reload (SIGHUP) service by name\n"  
		"\n"
		"  cond     set   <COND>     Set (assert) user-defined condition     +usr/COND\n"
		"  cond     clear <COND>     Clear (deassert) user-defined condition -usr/COND\n"
		"  cond     status           Show condition status, default cond command\n"
		"  cond     dump             Dump all conditions and their status\n"
		"\n"
		"  log      [NAME]           Show ten last Finit, or NAME, messages from syslog\n"
		"  start    <NAME>[:ID]      Start service by name, with optional ID\n"
		"  stop     <NAME>[:ID]      Stop/Pause a running service by name\n"
		"  restart  <NAME>[:ID]      Restart (stop/start) service by name\n"
		"  status   <NAME>[:ID]      Show service status, by name\n"
		"  status                    Show status of services, default command\n"
		"\n"
		"  ps                        List processes based on cgroups\n"
		"\n"
		"  runlevel [0-9]            Show or set runlevel: 0 halt, 6 reboot\n"
		"  reboot                    Reboot system\n"
		"  halt                      Halt system\n"
		"  poweroff                  Halt and power off system\n"
		"  suspend                   Suspend system\n");

	if (has_utmp())
		fprintf(stderr,
			"\n"
			"  utmp     show             Raw dump of UTMP/WTMP db\n"
			"\n");
	else
		fprintf(stderr, "\n");

	return rc;
}

static int do_devel(char *arg)
{
	(void)arg;

	printf("Screen %dx%d\n", screen_cols, screen_rows);

	return 0;
}

static int do_help(char *arg)
{
	return usage(0);
}

static int cmd_parse(int argc, char *argv[], struct cmd *command)
{
	int i, j;

	for (i = 0; argc > 0 && command[i].cmd; i++) {
		if (!string_match(command[i].cmd, argv[0]))
			continue;

		if (command[i].ctx)
			return cmd_parse(argc - 1, &argv[1], command[i].ctx);

		if (command[i].cb) {
			int rc = 0;

			if (argc == 1)
				return command[i].cb(NULL);

			for (j = 1; j < argc; j++)
				rc |= command[i].cb(argv[j]);

			return rc;
		}
	}

	return command[0].cb(NULL);	/* default cmd */
}

int main(int argc, char *argv[])
{
	struct option long_options[] = {
		{ "batch",      0, NULL, 'b' },
		{ "create",     0, NULL, 'c' },
		{ "force",      0, NULL, 'f' },
		{ "help",       0, NULL, 'h' },
		{ "plain",      0, NULL, 'p' },
		{ "no-heading", 0, NULL, 't' },
		{ "verbose",    0, NULL, 'v' },
		{ NULL, 0, NULL, 0 }
	};
	struct cmd cond[] = {
		{ "status",   NULL, do_cond_show }, /* default cmd */
		{ "dump",     NULL, do_cond_dump },
		{ "set",      NULL, do_cond_set  },
		{ "clr",      NULL, do_cond_clr  },
		{ "clear",    NULL, do_cond_clr  },
		{ NULL, NULL, NULL }
	};
	struct cmd command[] = {
		{ "status",   NULL, show_status  }, /* default cmd */

		{ "debug",    NULL, toggle_debug },
		{ "devel",    NULL, do_devel     },
		{ "help",     NULL, do_help      },
		{ "version",  NULL, show_version },

		{ "list",     NULL, serv_list    },
		{ "ls",       NULL, serv_list    },
		{ "enable",   NULL, serv_enable  },
		{ "disable",  NULL, serv_disable },
		{ "touch",    NULL, serv_touch   },
		{ "show",     NULL, serv_show    },
		{ "edit",     NULL, serv_edit    },
		{ "create",   NULL, serv_creat   },
		{ "delete",   NULL, serv_delete  },
		{ "reload",   NULL, do_reload    },

		{ "cond",     cond, NULL         },

		{ "log",      NULL, do_log       },
		{ "start",    NULL, do_start     },
		{ "stop",     NULL, do_stop      },
		{ "restart",  NULL, do_restart   },

		{ "ps",       NULL, show_cgroup  },

		{ "runlevel", NULL, do_runlevel  },
		{ "reboot",   NULL, do_reboot    },
		{ "halt",     NULL, do_halt      },
		{ "poweroff", NULL, do_poweroff  },
		{ "suspend",  NULL, do_suspend   },

		{ "utmp",     NULL, do_utmp      },
		{ NULL, NULL, NULL }
	};
	int interactive = 1, c;

	if (transform(progname(argv[0])))
		return reboot_main(argc, argv);

	while ((c = getopt_long(argc, argv, "bcfh?ptv", long_options, NULL)) != EOF) {
		switch(c) {
		case 'b':
			interactive = 0;
			break;

		case 'c':
			icreate = 1;
			break;

		case 'f':
			iforce = 1;
			break;

		case 'h':
		case '?':
			return usage(0);

		case 'p':
			plain = 1;
			break;

		case 't':
			heading = 0;
			break;

		case 'v':
			verbose = 1;
			break;
		}
	}

	if (interactive)
		screen_init();

	if (!has_utmp()) {
		/* system w/o utmp support, disable 'utmp' command */
		for (c = 0; command[c].cmd; c++) {
			if (!string_compare(command[c].cmd, "utmp"))
				continue;

			command[c].cb = NULL;
			break;
		}
	}

	return cmd_parse(argc - optind, &argv[optind], command);
}

void logit(int prio, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (prio <= LOG_ERR)
		verrx(1, fmt, ap);
	else
		vwarnx(fmt, ap);
	va_end(ap);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
