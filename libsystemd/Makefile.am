# libsystemd replacement library
lib_LTLIBRARIES          = libsystemd.la
libsystemd_la_SOURCES    = sd-daemon.c sd-daemon.h
libsystemd_la_LDFLAGS    = -version-info 0:0:0
libsystemd_la_CFLAGS     = $(lite_CFLAGS)
libsystemd_la_LIBADD     = $(lite_LIBS)

# pkg-config support
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsystemd.pc

# Install header to systemd/ subdirectory
systemddir = $(includedir)/systemd
systemd_HEADERS = sd-daemon.h
