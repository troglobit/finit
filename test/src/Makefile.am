noinst_PROGRAMS = serv

serv_SOURCES    = serv.c
serv_CPPFLAGS   = -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE -I$(top_builddir)
if LIBSYSTEMD
serv_CPPFLAGS  += -I$(top_srcdir)/libsystemd $(lite_CFLAGS)
serv_SOURCES   += $(top_srcdir)/libsystemd/sd-daemon.c
serv_LDADD      = $(lite_LIBS)
endif
