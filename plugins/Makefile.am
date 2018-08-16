pkglibdir           = $(plugin_path)
AM_LDFLAGS          = -module -avoid-version -shared
AM_CFLAGS           = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
AM_CPPFLAGS         = -I$(top_srcdir)/src -U_FORTIFY_SOURCE
AM_CPPFLAGS        += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS        += $(lite_CFLAGS)

if STATIC
noinst_LTLIBRARIES  = libplug.la
libplug_la_SOURCES  = bootmisc.c hwclock.c initctl.c pidfile.c procps.c tty.c urandom.c

if BUILD_ALSA_UTILS_PLUGIN
libplug_la_SOURCES += alsa-utils.c
endif

if BUILD_DBUS_PLUGIN
libplug_la_SOURCES += dbus.c
endif

if INETD
if BUILD_INETD_ECHO_PLUGIN
libplug_la_SOURCES += echo.c
endif

if BUILD_INETD_CHARGEN_PLUGIN
libplug_la_SOURCES += chargen.c
endif

if BUILD_INETD_DAYTIME_PLUGIN
libplug_la_SOURCES += daytime.c
endif

if BUILD_INETD_DISCARD_PLUGIN
libplug_la_SOURCES += discard.c
endif

if BUILD_INETD_TIME_PLUGIN
libplug_la_SOURCES += time.c
endif
endif

if BUILD_MODULES_LOAD_PLUGIN
libplug_la_SOURCES += modules-load.c
endif

if BUILD_NETLINK_PLUGIN
libplug_la_SOURCES += netlink.c
endif

if BUILD_RESOLVCONF_PLUGIN
libplug_la_SOURCES += resolvconf.c
endif

else
pkglib_LTLIBRARIES  = bootmisc.la hwclock.la initctl.la pidfile.la procps.la tty.la urandom.la

if BUILD_ALSA_UTILS_PLUGIN
pkglib_LTLIBRARIES += alsa-utils.la
endif

if BUILD_DBUS_PLUGIN
pkglib_LTLIBRARIES += dbus.la
endif

if INETD
if BUILD_INETD_ECHO_PLUGIN
pkglib_LTLIBRARIES += echo.la
endif

if BUILD_INETD_CHARGEN_PLUGIN
pkglib_LTLIBRARIES += chargen.la
endif

if BUILD_INETD_DAYTIME_PLUGIN
pkglib_LTLIBRARIES += daytime.la
endif

if BUILD_INETD_DISCARD_PLUGIN
pkglib_LTLIBRARIES += discard.la
endif

if BUILD_INETD_TIME_PLUGIN
pkglib_LTLIBRARIES += time.la
endif
endif

if BUILD_MODULES_LOAD_PLUGIN
pkglib_LTLIBRARIES += modules-load.la
endif

if BUILD_NETLINK_PLUGIN
pkglib_LTLIBRARIES += netlink.la
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

uninstall-hook:
	@(cd $(DESTDIR)$(pkglibdir) && for file in $(pkglib_LTLIBRARIES); do $(RM) `basename $$file .la`.so; done)
endif
