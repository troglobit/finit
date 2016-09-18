ACLOCAL_AMFLAGS    = -I m4
sbin_PROGRAMS      = finit initctl reboot
finit_SOURCES      = api.c					\
		     cond.c	cond-w.c	cond.h		\
		     client.c					\
		     conf.c	conf.h				\
		     exec.c	finit.c		finit.h		\
		     getty.c					\
		     helpers.c	helpers.h			\
		     mdadm.c	mount.c		pid.c		\
		     plugin.c	plugin.h	private.h	\
		     service.c	service.h			\
		     sig.c	sig.h				\
		     sm.c	sm.h				\
		     svc.c	svc.h				\
		     tty.c	tty.h				\
		     utmp-api.c	utmp-api.h
pkginclude_HEADERS = cond.h finit.h helpers.h inetd.h plugin.h svc.h
if INETD
finit_SOURCES     += inetd.c	inetd.h
endif

finit_CFLAGS       = -W -Wall -Wextra -std=gnu99 $(lite_CFLAGS) $(uev_CFLAGS)
finit_LDADD        = $(lite_LIBS) $(uev_LIBS)
if STATIC
finit_LDADD       += plugins/libplug.la
else
finit_LDADD       += -ldl
endif

initctl_SOURCES    = initctl.c svc.c svc.h helpers.c helpers.h cond.c cond.h \
		     utmp-api.c utmp-api.h
initctl_CFLAGS     = $(lite_CFLAGS)
initctl_LDADD      = $(lite_LIBS)

reboot_SOURCES     = reboot.c

INIT_LNKS          = telinit init
REBOOT_LNKS        = shutdown halt poweroff suspend
SUBDIRS            = plugins
doc_DATA           = README.md LICENSE finit.conf
EXTRA_DIST         = README.md LICENSE ChangeLog.md finit.conf
AM_CPPFLAGS        = -U_FORTIFY_SOURCE
AM_CPPFLAGS       += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE

if STATIC
AM_CPPFLAGS       += -DENABLE_STATIC
AM_LDFLAGS         = -static-libtool-libs
else
AM_CPPFLAGS       += -DPLUGIN_PATH=\"$(pkglibdir)\"
AM_LDFLAGS         = -export-dynamic
endif

# Compat hook
install-dev: install-pkgincludeHEADERS

# Hook in install to add finit and reboot symlink(s)
install-exec-hook:
	@for file in $(INIT_LNKS); do				\
		lnk=$(DESTDIR)$(sbindir)/$$file;		\
		if [ -e $$lnk ]; then				\
			echo "Skipping $$lnk exists already.";	\
		else						\
			$(LN_S) finit $$lnk;			\
		fi;						\
	done
	@if [ -L $(DESTDIR)$(sbindir)/reboot ]; then		\
		echo "Non-finit reboot, skipping.";		\
	else							\
		for file in $(REBOOT_LNKS); do			\
			link=$(DESTDIR)$(sbindir)/$$file;	\
			if [ -e $$link ]; then			\
				echo "$$link exists, skipping.";\
			else					\
				$(LN_S) reboot $$link;		\
			fi;					\
		done;						\
	fi

uninstall-hook:
	@for file in $(INIT_LNKS); do				\
		lnk=`readlink $(DESTDIR)$(sbindir)/$$file`;	\
		if [ "$$lnk" != "finit" ]; then			\
			echo "Skipping non-finit $$file.";	\
			continue;				\
		fi;						\
		$(RM) $(DESTDIR)$(sbindir)/$$file;		\
	done
	@if [ -L $(DESTDIR)$(sbindir)/reboot ]; then		\
		echo "Non-finit reboot, skipping uninstall.";	\
	else							\
		for file in $(REBOOT_LNKS); do			\
			if [ "`readlink $$file`" != "reboot" ]; \
				then continue; 			\
			fi;					\
			$(RM) $(DESTDIR)$(sbindir)/$$file;	\
		done;						\
	fi

# Target to run when building a release
release: dist
	@for file in $(DIST_ARCHIVES); do	\
		md5sum $$file > ../$$file.md5;	\
	done
	@mv $(DIST_ARCHIVES) ../
	@echo
	@echo "Resulting release files:"
	@echo "================================================================="
	@for file in $(DIST_ARCHIVES); do					\
		printf "$$file    \tDistribution tarball\n";			\
		printf "$$file.md5\t"; cat ../$$file.md5 | cut -f1 -d' ';	\
	done
