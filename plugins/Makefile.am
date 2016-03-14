AM_LDFLAGS          = -module -avoid-version -shared -no-undefined -export-dynamic
AM_CPPFLAGS         = -U_FORTIFY_SOURCE
AM_CPPFLAGS        += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS        += -DFINIT_FIFO=\"$(fifo)\"

if STATIC
noinst_LTLIBRARIES  = libplug.la
libplug_la_SOURCES  = alsa-utils.c bootmisc.c dbus.c hwclock.c		\
		      initctl.c netlink.c pidfile.c resolvconf.c	\
		      time.c tty.c urandom.c x11-common.c
else
pkglib_LTLIBRARIES  = alsa-utils.la bootmisc.la dbus.la hwclock.la	\
		      initctl.la netlink.la pidfile.la resolvconf.la	\
		      time.la tty.la urandom.la x11-common.la

# Clean up .la files installed by libtool despite being in -module mode
install-exec-hook:
	@(cd $(DESTDIR)$(pkglibdir) && $(RM) $(pkglib_LTLIBRARIES))
endif
