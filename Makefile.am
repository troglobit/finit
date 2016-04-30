ACLOCAL_AMFLAGS    = -I m4
sbin_PROGRAMS      = finit initctl reboot
finit_SOURCES      = finit.c api.c client.c conf.c exec.c helpers.c pid.c \
		     sig.c svc.c service.c plugin.c tty.c cond.c cond-w.c
pkginclude_HEADERS = cond.h finit.h helpers.h inetd.h plugin.h queue.h svc.h
if INETD
finit_SOURCES      += inetd.c
endif
finit_CFLAGS       = -W -Wall -Wextra -std=gnu99 $(lite_CFLAGS) $(uev_CFLAGS)
finit_LDADD        = -ldl $(lite_LIBS) $(uev_LIBS)
if STATIC
finit_LDADD       += plugins/libplug.la
endif
initctl_SOURCES    = initctl.c svc.c helpers.c cond.c
initctl_CFLAGS     = $(lite_CFLAGS)
initctl_LDADD      = $(lite_LIBS)
reboot_SOURCES     = reboot.c
SYMLINK            = telinit init
SUBDIRS            = plugins
doc_DATA           = README.md LICENSE finit.conf services
EXTRA_DIST         = README.md LICENSE ChangeLog.md finit.conf services
AM_CPPFLAGS        = -U_FORTIFY_SOURCE
AM_CPPFLAGS       += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS       += -DFINIT_CONF=\"$(conf)\" -DFINIT_RCSD=\"$(rcsd)\" -DFINIT_FIFO=\"$(fifo)\" \
		     -DPLUGIN_PATH=\"$(pkglibdir)\"
AM_LDFLAGS         = -export-dynamic


# Compat hook
install-dev: install-pkgincludeHEADERS

# Hook in install to add finit --> telinit symlink(s)
install-exec-hook:
	@for file in $(SYMLINK); do				\
		link=$(DESTDIR)$(sbindir)/$$file;		\
		test "`readlink $$link`" = "finit" && continue;	\
		$(LN_S) finit $$link;				\
	done

uninstall-hook:
	@for file in $(SYMLINK); do				\
		$(RM) $(DESTDIR)$(sbindir)/$$file;		\
	done

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
