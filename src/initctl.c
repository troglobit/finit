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
#include <getopt.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <utmp.h>
#include <arpa/inet.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif

#include "client.h"
#include "cond.h"
#include "serv.h"
#include "service.h"
#include "cgutil.h"
#include "util.h"
#include "utmp-api.h"

struct cmd {
	char        *cmd;
	struct cmd  *ctx;
	int        (*cb)(char *arg);
	int         *cond;
};

int icreate  = 0;
int iforce   = 0;
int ionce    = 0;
int debug    = 0;
int heading  = 1;
int verbose  = 0;
int plain    = 0;
int quiet    = 0;
int runlevel = 0;
int cgrp     = 0;
int utmp     = 0;

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
		for (len = 0; len < ttcols; len++)
			fputc('=', stdout);
		fputs("\n", stdout);
	} else {
		char buf[ttcols];

		vsnprintf(buf, sizeof(buf), fmt, ap);
		printheader(stdout, buf, 0);
	}

        va_end(ap);
}

static int runlevel_get(int *prevlevel)
{
	struct init_request rq;
	int rc;

	rq.cmd = INIT_CMD_GET_RUNLEVEL;
	rq.magic = INIT_MAGIC;

	rc = client_send(&rq, sizeof(rq));
	if (!rc) {
		rc = rq.runlevel;
		if (prevlevel)
			*prevlevel = rq.sleeptime;
	}

	return rc;
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
	char *logfile = "/var/log/syslog";

	if (!svc || !svc[0])
		svc = "finit";

	if (!fexist(logfile))
		logfile = "/var/log/messages";

	return systemf("cat %s | grep %s | tail -10", logfile, svc);
}

static int do_runlevel(char *arg)
{
	struct init_request rq = {
		.magic = INIT_MAGIC,
		.cmd = INIT_CMD_RUNLVL,
	};

	if (!arg) {
		int prevlevel = 0;
		int currlevel;
		char prev;

		currlevel = runlevel_get(&prevlevel);
		if (255 == currlevel) {
			printf("unknown\n");
			return 0;
		}

		prev = prevlevel + '0';
		if (prev <= '0' || prev > '9')
			prev = 'N';

		printf("%c %d\n", prev , currlevel);
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
		.cmd   = cmd,
		.data  = "",
	};

	if (arg)
		strlcpy(rq.data, arg, sizeof(rq.data));

	return client_send(&rq, sizeof(rq));
}

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

static int do_reload (char *arg)
{
	if (!arg || !arg[0])
		return do_svc(INIT_CMD_RELOAD, NULL);

	return do_startstop(INIT_CMD_RELOAD_SVC, arg);
}

static int do_restart(char *arg)
{
	size_t retries = 3;
	svc_t *svc;

	if (do_startstop(INIT_CMD_STOP_SVC, arg))
		return 1;

	while (retries-- > 0 && (svc = client_svc_find(arg))) {
		if (!svc_is_running(svc))
			break;

		sleep(1);
	}

	if (retries == 0)
		errx(1, "Failed stopping %s (restart)", arg);

	return do_startstop(INIT_CMD_RESTART_SVC, arg);
}

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

		svc = client_svc_find_by_cond(cond);
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

	printf("%-*d  %-*s  %-6s  <%s>\n", pw, pid, iw, nm, asserted, cond);

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
		if (erase(oneshot) && errno != ENOENT)
			err(1, "Failed deasserting condition <%s>", &oneshot[off]);
	}

	return 0;
}

static int do_cond_set(char *arg) { return do_cond_act(arg, 1); }
static int do_cond_clr(char *arg) { return do_cond_act(arg, 0); }

static char *svc_cond(svc_t *svc, char *buf, size_t len)
{
	char *cond, *conds;

	buf[0] = 0;

	if (!svc->cond[0])
		return buf;

	conds = strdupa(svc->cond);
	if (!conds)
		return buf;

	strlcat(buf, "<", len);

	for (cond = strtok(conds, ","); cond; cond = strtok(NULL, ",")) {
		if (cond != conds)
			strlcat(buf, ",", len);

		switch (cond_get(cond)) {
		case COND_ON:
			strlcat(buf, "+", len);
			strlcat(buf, cond, len);
			break;

		case COND_FLUX:
			if (!plain)
				strlcat(buf, "\e[1m", len);
			strlcat(buf, "~", len);
			strlcat(buf, cond, len);
			if (!plain)
				strlcat(buf, "\e[0m", len);
			break;

		case COND_OFF:
			if (!plain)
				strlcat(buf, "\e[1m", len);
			strlcat(buf, "-", len);
			strlcat(buf, cond, len);
			if (!plain)
				strlcat(buf, "\e[0m", len);
			break;
		}
	}

	strlcat(buf, ">", len);

	return buf;
}

static int do_cond_show(char *arg)
{
	enum cond_state cond;
	char buf[512];
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

		svc_ident(svc, buf, sizeof(buf));
		printf("%-*d  %-*s  ", pw, svc->pid, iw, buf);

		if (cond == COND_ON)
			printf("%-6.6s  ", condstr(cond));
		else
			printf("\e[1m%-6.6s\e[0m  ", condstr(cond));

		puts(svc_cond(svc, buf, sizeof(buf)));
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
	struct utmp *ut;
	time_t sec;

	if (heading) {
		static int once = 0;

		print_header("%s%s ", once ? "\n" : "", file);
		once++;
	}
	utmpname(file);

	setutent();
	while ((ut = getutent())) {
		char user[sizeof(ut->ut_user) + 1];
		char id[sizeof(ut->ut_id) + 1];
		struct tm *sectm;
		char when[80];
		char addr[64];
		int pid;

		strlcpy(id, ut->ut_id, sizeof(id));
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
	if (!utmp)
		return 1;

	if (fexist(file))
		return utmp_show(file);

	return  utmp_show(_PATH_WTMP) ||
		utmp_show(_PATH_UTMP);
}

static int show_version(char *arg)
{
	puts(PACKAGE_STRING);
	printf("Bug report address: %-40s\n", PACKAGE_BUGREPORT);
#ifdef PACKAGE_URL
	printf("Project homepage: %s\n", PACKAGE_URL);
#endif

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
char *runlevel_string(int currlevel, int levels)
{
	int i, pos = 1;
	static char lvl[21];

	memset(lvl, 0, sizeof(lvl));
	lvl[0] = '[';

	for (i = 0; i < 10; i++) {
		if (ISSET(levels, i)) {
			if (!plain && currlevel == i)
				pos = strlcat(lvl, "\e[1m", sizeof(lvl));

			if (i == 0)
				lvl[pos++] = 'S';
			else
				lvl[pos++] = '0' + i;

			if (!plain && currlevel == i)
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

	if (plain || svc_checkenv(svc) || svc->env[0] == '-')
		bold = 0;

	strlcpy(buf, bold ? "\e[1m" : "", len);
	strlcat(buf, svc->env, len);
	strlcat(buf, bold ? "\e[0m" : "", len);

	return buf;
}

static char *exit_status(int status, char *buf, size_t len)
{
	int rc, sig;

	rc = WEXITSTATUS(status);
	sig = WTERMSIG(status);

	if (WIFEXITED(status))
		snprintf(buf, len, " (code=exited, status=%d%s)", rc, code2str(rc));
	else if (WIFSIGNALED(status))
		snprintf(buf, len, " (code=signal, status=%d%s)", sig, sig2str(sig));

	return buf;
}

static char *status(svc_t *svc, int full)
{
	static char buf[96];
	const char *color;
	char ok[48] = {0};
	char *s;

	s = svc_status(svc);
	switch (svc->state) {
	case SVC_HALTED_STATE:
		exit_status(svc->status, ok, sizeof(ok));
		color = "\e[1m";
		break;

	case SVC_RUNNING_STATE:
		color = "\e[1;32m";
		break;

	case SVC_DONE_STATE:
		exit_status(svc->status, ok, sizeof(ok));
		if (WIFEXITED(svc->status)) {
			if (WEXITSTATUS(svc->status))
				color = "\e[1;31m";
			else
				color = "\e[1;32m";
		} else {
			if (full && WIFSIGNALED(svc->status))
				color = "\e[1;31m";
			else
				color = "\e[1;33m";
		}
		break;

	default:
		exit_status(svc->status, ok, sizeof(ok));
		color = "\e[1;33m";
		break;
	}

	if (!full || plain)
		color = NULL;

	if (!full)
		snprintf(buf, sizeof(buf), "%-8.8s", s);
	else
		snprintf(buf, sizeof(buf), "%s%s%s%s",
			 color ? color : "", s, ok, color ? "\e[0m" : "");

	return buf;
}

static void show_cgroup_tree(char *group, char *pfx)
{
	char path[256];

	if (!group) {
		puts("");
		return;
	}

	strlcpy(path, FINIT_CGPATH, sizeof(path));
	strlcat(path, group, sizeof(path));

	cgroup_tree(path, pfx, 0, 0);
}

static int show_status(char *arg)
{
	char ident[MAX_IDENT_LEN];
	char buf[512];
	int num = 0;
	svc_t *svc;

	runlevel = runlevel_get(NULL);

	while (arg && arg[0]) {
		long now = jiffies();
		char uptm[42] = "N/A";
		char *pidfn = NULL;
		int exact = 0;

		for (svc = client_svc_iterator(1); svc; svc = client_svc_iterator(0)) {
			svc_ident(svc, ident, sizeof(ident));
			if (string_compare(ident, arg))
				num++;
			if (string_case_compare(ident, arg))
				exact++;
		}

		if (num > 1 && !exact)
			break;

		svc = client_svc_find(arg);
		if (!svc)
			return 255;

		if (quiet)
			return svc->state != SVC_RUNNING_STATE;

		pidfn = svc->pidfile;
		if (pidfn[0] == '!')
			pidfn++;
		else if (pidfn[0] == 0)
			pidfn = "none";

		printf("     Status : %s\n", status(svc, 1));
		printf("   Identity : %s\n", svc_ident(svc, ident, sizeof(ident)));
		printf("Description : %s\n", svc->desc);
		printf("     Origin : %s\n", svc->file[0] ? svc->file : "built-in");
		printf("Environment : %s\n", svc_environ(svc, buf, sizeof(buf)));
		printf("Condition(s): %s\n", svc_cond(svc, buf, sizeof(buf)));
		printf("    Command : %s\n", svc_command(svc, buf, sizeof(buf)));
		printf("   PID file : %s\n", pidfn);
		printf("        PID : %d\n", svc->pid);
		printf("       User : %s\n", svc->username);
		printf("      Group : %s\n", svc->group);
		printf("     Uptime : %s\n", svc->pid ? uptime(now - svc->start_time, uptm, sizeof(uptm)) : uptm);
		printf("   Restarts : %d (%d/%d)\n", svc->restart_tot, svc->restart_cnt, svc->restart_max);
		printf("  Runlevels : %s\n", runlevel_string(runlevel, svc->runlevels));
		if (cgrp && svc->pid > 1) {
			char grbuf[128];
			char path[256];
			struct cg *cg;
			char *group;

			group = pid_cgroup(svc->pid, grbuf, sizeof(grbuf));
			snprintf(path, sizeof(path), "%s/%s", FINIT_CGPATH, group);
			cg = cg_conf(path);

			printf("     Memory : %s\n", memsz(cgroup_memory(group), uptm, sizeof(uptm)));
			printf("     CGroup : %s cpu %s [%s, %s] mem [%s, %s]\n",
			       group, cg->cg_cpu.set, cg->cg_cpu.weight, cg->cg_cpu.max,
			       cg->cg_mem.min, cg->cg_mem.max);
			show_cgroup_tree(group, "              ");
		}
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

		svc_ident(svc, ident, sizeof(ident));
		if (num && !string_compare(ident, arg))
			continue;

		printf("%-*d  ", pw, svc->pid);
		printf("%-*s  %s ", iw, ident, status(svc, 0));

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

static int show_ident(char *arg)
{
	svc_t *svc;

	for (svc = client_svc_iterator(1); svc; svc = client_svc_iterator(0)) {
		char ident[MAX_IDENT_LEN];
		size_t len;
		char *pos;

		svc_ident(svc, ident, sizeof(ident));
		pos = strchr(ident, ':');
		if (pos)
			len = pos - ident;
		else
			len = strlen(ident);
		if (arg && arg[0] && strncasecmp(ident, arg, len))
			continue;

		puts(ident);
	}

	return 0;
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
		"  -1, --once                Only one lap in commands like 'top'\n"
		"  -p, --plain               Use plain table headings, no ctrl chars\n"
		"  -q, --quiet               Silent, only return status of command\n"
		"  -t, --no-heading          Skip table headings\n"
		"  -v, --verbose             Verbose output\n"
		"  -R, --version             Show program version\n"
		"  -h, --help                This help text\n"
		"\n"
		"Commands:\n"
		"  debug                     Toggle Finit (daemon) debug\n"
		"  help                      This help text\n"
		"  version                   Show program version\n"
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
		"  reload   <NAME>[:ID]      Reload service by name (SIGHUP or restart)\n"
		"  restart  <NAME>[:ID]      Restart (stop/start) service by name\n"
		"  ident    [NAME]           Show matching identities for NAME, or all\n"
		"  status   <NAME>[:ID]      Show service status, by name\n"
		"  status                    Show status of services, default command\n");
	if (cgrp)
		fprintf(stderr,
			"\n"
			"  cgroup                    List cgroup config overview\n"
			"  ps                        List processes based on cgroups\n"
			"  top                       Show top-like listing based on cgroups\n");
	fprintf(stderr,
		"\n"
		"  runlevel [0-9]            Show or set runlevel: 0 halt, 6 reboot\n"
		"  reboot                    Reboot system\n"
		"  halt                      Halt system\n"
		"  poweroff                  Halt and power off system\n"
		"  suspend                   Suspend system\n");

	if (utmp)
		fprintf(stderr,
			"\n"
			"  utmp     show             Raw dump of UTMP/WTMP db\n");
	fprintf(stderr, "\n");

	return rc;
}

static int do_devel(char *arg)
{
	(void)arg;

	printf("Screen %dx%d\n", ttcols, ttrows);

	return 0;
}

static int do_help(char *arg)
{
	return usage(0);
}

static int cmd_cond(struct cmd *cmd)
{
	if (!cmd || !cmd->cond)
		return 1;

	return *cmd->cond;
}

static int cmd_parse(int argc, char *argv[], struct cmd *command)
{
	int i, j;

	for (i = 0; argc > 0 && command[i].cmd; i++) {
		if (!cmd_cond(&command[i]))
			continue;

		if (!string_compare(command[i].cmd, argv[0]))
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

	if (argv[0] && strlen(argv[0]) > 0)
		errx(1, "No such command.  See 'initctl help' for an overview of available commands.");

	return command[0].cb(NULL); /* default cmd */
}
int main(int argc, char *argv[])
{
	struct option long_options[] = {
		{ "batch",      0, NULL, 'b' },
		{ "create",     0, NULL, 'c' },
		{ "debug",      0, NULL, 'd' },
		{ "force",      0, NULL, 'f' },
		{ "help",       0, NULL, 'h' },
		{ "once",       0, NULL, '1' },
		{ "plain",      0, NULL, 'p' },
		{ "quiet",      0, NULL, 'q' },
		{ "no-heading", 0, NULL, 't' },
		{ "verbose",    0, NULL, 'v' },
		{ "version",    0, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct cmd cond[] = {
		{ "status",   NULL, do_cond_show, NULL }, /* default cmd */
		{ "dump",     NULL, do_cond_dump, NULL },
		{ "set",      NULL, do_cond_set,  NULL },
		{ "clr",      NULL, do_cond_clr,  NULL },
		{ "clear",    NULL, do_cond_clr,  NULL },
		{ NULL, NULL, NULL, NULL }
	};
	struct cmd command[] = {
		{ "status",   NULL, show_status,  NULL }, /* default cmd */
		{ "ident",    NULL, show_ident,   NULL },

		{ "debug",    NULL, toggle_debug, NULL },
		{ "devel",    NULL, do_devel,     NULL },
		{ "help",     NULL, do_help,      NULL },
		{ "version",  NULL, show_version, NULL },

		{ "list",     NULL, serv_list,    NULL },
		{ "ls",       NULL, serv_list,    NULL },
		{ "enable",   NULL, serv_enable,  NULL },
		{ "disable",  NULL, serv_disable, NULL },
		{ "touch",    NULL, serv_touch,   NULL },
		{ "show",     NULL, serv_show,    NULL },
		{ "edit",     NULL, serv_edit,    NULL },
		{ "create",   NULL, serv_creat,   NULL },
		{ "delete",   NULL, serv_delete,  NULL },
		{ "reload",   NULL, do_reload,    NULL },

		{ "cond",     cond, NULL, NULL         },

		{ "log",      NULL, do_log,       NULL },
		{ "start",    NULL, do_start,     NULL },
		{ "stop",     NULL, do_stop,      NULL },
		{ "restart",  NULL, do_restart,   NULL },

		{ "cgroup",   NULL, show_cgroup, &cgrp },
		{ "ps",       NULL, show_cgps,   &cgrp },
		{ "top",      NULL, show_cgtop,  &cgrp },

		{ "runlevel", NULL, do_runlevel,  NULL },
		{ "reboot",   NULL, do_reboot,    NULL },
		{ "halt",     NULL, do_halt,      NULL },
		{ "poweroff", NULL, do_poweroff,  NULL },
		{ "suspend",  NULL, do_suspend,   NULL },

		{ "utmp",     NULL, do_utmp,     &utmp },
		{ NULL, NULL, NULL, NULL }
	};
	int interactive = 1, c;

	if (transform(progname(argv[0])))
		return reboot_main(argc, argv);

	/* Enable functionality depending on system capabilities */
	cgrp = cgroup_avail();
	utmp = has_utmp();

	while ((c = getopt_long(argc, argv, "1bcdfh?pqtvV", long_options, NULL)) != EOF) {
		switch(c) {
		case '1':
			ionce = 1;
			break;

		case 'b':
			interactive = 0;
			break;

		case 'c':
			icreate = 1;
			break;

		case 'd':
			debug = 1;
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

		case 'q':
			quiet = 1;
			break;

		case 't':
			heading = 0;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'V':
			return show_version(NULL);
		}
	}

	if (interactive)
		ttinit();

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
