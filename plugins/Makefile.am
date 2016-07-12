AM_LDFLAGS          = -module -avoid-version -shared
AM_CPPFLAGS         = -U_FORTIFY_SOURCE
AM_CPPFLAGS        += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS        += -DFINIT_FIFO=\"$(fifo)\" $(lite_CFLAGS)

if STATIC
noinst_LTLIBRARIES  = libplug.la
libplug_la_SOURCES  = bootmisc.c hwclock.c initctl.c netlink.c pidfile.c	\
		      time.c tty.c urandom.c

if BUILD_ALSA_UTILS_PLUGIN
libplug_la_SOURCES += alsa-utils.c
endif

if BUILD_DBUS_PLUGIN
libplug_la_SOURCES += dbus.c
endif

if BUILD_INETD_ECHO_PLUGIN
libplug_la_SOURCES += echo.c
endif

if BUILD_LOST_PLUGIN
libplug_la_SOURCES += lost.c
endif

if BUILD_RESOLVCONF_PLUGIN
libplug_la_SOURCES += resolvconf.c
endif

else
pkglib_LTLIBRARIES  = bootmisc.la hwclock.la initctl.la netlink.la pidfile.la	\
		      time.la tty.la urandom.la

if BUILD_ALSA_UTILS_PLUGIN
pkglib_LTLIBRARIES += alsa-utils.la
endif

if BUILD_DBUS_PLUGIN
pkglib_LTLIBRARIES += dbus.la
endif

if BUILD_INETD_ECHO_PLUGIN
pkglib_LTLIBRARIES += echo.la
endif

if BUILD_LOST_PLUGIN
pkglib_LTLIBRARIES += lost.la
endif

if BUILD_RESOLVCONF_PLUGIN
pkglib_LTLIBRARIES += resolvconf.la
endif

if BUILD_X11_COMMON_PLUGIN
pkglib_LTLIBRARIES += x11-common.la
endif

# Clean up .la files installed by libtool despite being in -module mode
install-exec-hook:
	@(cd $(DESTDIR)$(pkglibdir) && $(RM) $(pkglib_LTLIBRARIES))
endif
