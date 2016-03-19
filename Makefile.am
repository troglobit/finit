ACLOCAL_AMFLAGS    = -I m4
sbin_PROGRAMS      = finit initctl reboot
finit_SOURCES      = finit.c api.c client.c conf.c exec.c helpers.c pid.c \
		     sig.c svc.c service.c plugin.c tty.c cond.c cond-w.c
if INETD
finit_SOURCES      += inetd.c
endif
finit_CFLAGS       = -W -Wall -Wextra -std=gnu99
finit_LDADD        = libite/.libs/libite.la libuev/.libs/libuev.la -ldl
if STATIC
finit_LDADD       += plugins/libplug.la
endif
DEPENDENCIES       = libite/libite.la libuev/libuev.la
initctl_SOURCES    = initctl.c svc.c helpers.c cond.c
initctl_LDADD      = libite/.libs/libite.la
reboot_SOURCES     = reboot.c
SYMLINK            = telinit init
#DIST_SUBDIRS       = libite libuev
SUBDIRS            = libite libuev plugins
doc_DATA           = README.md LICENSE finit.conf services
EXTRA_DIST         = README.md LICENSE ChangeLog.md finit.conf services
AM_CPPFLAGS        = -U_FORTIFY_SOURCE
AM_CPPFLAGS       += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS       += -DFINIT_CONF=\"$(conf)\" -DFINIT_RCSD=\"$(rcsd)\" -DFINIT_FIFO=\"$(fifo)\" \
		     -DPLUGIN_PATH=\"$(plugindir)\" 

# Slightly ugly hacks to prevent the build system from installing
# libuev/libite, which it does if we use the more plain SUBDIRS.
#$(DEPENDENCIES):
#	@$(MAKE) -C `dirname $@` all
#
#$(initctl_LDADD) $(finit_LDADD): $(DEPENDENCIES)
#
#clean-local:
#	@-for dep in $(DEPENDENCIES); do			\
#		$(MAKE) -C `dirname $$dep` clean;		\
#	done

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
