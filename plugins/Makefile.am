pkglibdir           = $(plugin_path)
AM_LDFLAGS          = -module -avoid-version -shared
AM_CFLAGS           = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
AM_CPPFLAGS         = -I$(top_srcdir)/src -U_FORTIFY_SOURCE
AM_CPPFLAGS        += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
AM_CPPFLAGS        += $(lite_CFLAGS)

if STATIC
noinst_LTLIBRARIES  = libplug.la
libplug_la_SOURCES  = bootmisc.c modprobe.c rtc.c pidfile.c procps.c tty.c urandom.c usr.c

if BUILD_ALSA_UTILS_PLUGIN
libplug_la_SOURCES += alsa-utils.c
endif

if BUILD_DBUS_PLUGIN
libplug_la_SOURCES += dbus.c
endif

if BUILD_HOTPLUG_PLUGIN
libplug_la_SOURCES += hotplug.c
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
pkglib_LTLIBRARIES  = bootmisc.la modprobe.la rtc.la pidfile.la procps.la tty.la urandom.la usr.la

if BUILD_ALSA_UTILS_PLUGIN
pkglib_LTLIBRARIES += alsa-utils.la
endif

if BUILD_DBUS_PLUGIN
pkglib_LTLIBRARIES += dbus.la
endif

if BUILD_HOTPLUG_PLUGIN
pkglib_LTLIBRARIES += hotplug.la
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
