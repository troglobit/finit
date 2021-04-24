EXTRA_DIST         = rescue.conf sample.conf
AM_CPPFLAGS        = -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
if STATIC
AM_CPPFLAGS       += -DENABLE_STATIC
AM_LDFLAGS         = -static-libtool-libs
else
AM_LDFLAGS         = -export-dynamic
endif

sbin_PROGRAMS        = finit initctl
pkglibexec_PROGRAMS  = sulogin
finit_pkglibdir      = $(pkglibdir)
finit_pkglib_SCRIPTS = rescue.conf sample.conf

sulogin_SOURCES      = sulogin.c
sulogin_CFLAGS       = -W -Wall -Wextra -std=gnu99
sulogin_LDADD        = -lcrypt

if WATCHDOGD
pkglibexec_PROGRAMS += watchdogd
endif

if LOGIT
pkglibexec_PROGRAMS += logit
logit_SOURCES        = logit.c logrotate.c
logit_CFLAGS         = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
logit_CFLAGS        += $(lite_CFLAGS)
logit_LDADD          = $(lite_LIBS)
endif

finit_SOURCES      = api.c	cgroup.c	cgroup.h	\
		     cond.c	cond-w.c	cond.h		\
		     conf.c	conf.h				\
		     exec.c	finit.c		finit.h		\
		     getty.c	stty.c				\
		     helpers.c	helpers.h			\
		     iwatch.c   iwatch.h			\
		     log.c	log.h				\
		     mdadm.c	mount.c				\
		     pid.c      pid.h				\
		     plugin.c	plugin.h	private.h	\
		     schedule.c	schedule.h			\
		     service.c	service.h			\
		     sig.c	sig.h				\
		     sm.c	sm.h				\
		     svc.c	svc.h				\
		     tty.c	tty.h				\
		     util.c	util.h				\
		     utmp-api.c	utmp-api.h
if LOGROTATE
finit_SOURCES     += logrotate.c
endif

pkginclude_HEADERS = cgroup.h cond.h finit.h helpers.h log.h plugin.h svc.h

finit_CFLAGS       = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
finit_CFLAGS      += $(lite_CFLAGS) $(uev_CFLAGS)
finit_LDADD        = $(lite_LIBS) $(uev_LIBS)
if STATIC
finit_LDADD       += ../plugins/libplug.la
else
finit_LDADD       += -ldl
endif

initctl_SOURCES    = initctl.c initctl.h cgutil.c cgutil.h		\
		     client.c client.h cond.c cond.h reboot.c		\
		     serv.c serv.h svc.h util.c util.h
initctl_CFLAGS     = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
initctl_CFLAGS    += $(lite_CFLAGS) $(uev_CFLAGS)
initctl_LDADD      = $(lite_LIBS) $(uev_LIBS)

INIT_LNKS          = init telinit
REBOOT_LNKS        = reboot shutdown halt poweroff suspend

if WATCHDOGD
watchdogd_SOURCES  = watchdog.c watchdog.h
watchdogd_CFLAGS   = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
endif

# Hook in install to add finit and reboot symlink(s)
install-exec-hook:
	@for file in $(INIT_LNKS); do			\
		lnk=$(DESTDIR)$(sbindir)/$$file;	\
		if [ -e $$lnk ]; then			\
			echo "Skipping $$lnk exists.";	\
		else					\
			$(LN_S) finit $$lnk;		\
		fi;					\
	done
	@for file in $(REBOOT_LNKS); do			\
		lnk=$(DESTDIR)$(sbindir)/$$file;	\
		if [ -e $$lnk ]; then			\
			echo "Skipping $$lnk exists.";	\
		else					\
			$(LN_S) initctl $$lnk;		\
		fi;					\
	done

uninstall-hook:
	@for file in $(INIT_LNKS); do			\
		lnk=$(DESTDIR)$(sbindir)/$$file;	\
		dst=`readlink $$lnk`;			\
		if [ "$$dst" != "finit" ]; then		\
			continue;			\
		fi;					\
		$(RM) $(DESTDIR)$(sbindir)/$$file;	\
	done
	@for file in $(REBOOT_LNKS); do			\
	        lnk=$(DESTDIR)$(sbindir)/$$file;	\
		dst=`readlink $$lnk`;			\
		if [ "$$dst" != "initctl" ]; then 	\
			continue; 			\
		fi;					\
		$(RM) $$lnk;				\
	done
