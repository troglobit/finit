# Plugin based services architecture for finit -*-Makefile-*-
CFLAGS     += -fPIC -Os
CPPFLAGS   += -I..
LDFLAGS    := $(filter-out -rdynamic, $(LDFLAGS))
LDFLAGS    += -shared
PLUGINS    ?= initctl.so alsa-utils.so bootmisc.so dbus.so hwclock.so \
	      resolvconf.so urandom.so x11-common.so tty.so time.so
DEPS        = $(addprefix .,$(PLUGINS:.so=.d))
TOPDIR     ?= $(shell pwd)

include ../common.mk

all: $(PLUGINS)

clean:
	-@$(RM) $(PLUGINS) *.o

distclean: clean
	-@$(RM) $(JUNK) $(DEPS)

install: all
	@install -d $(DESTDIR)$(plugindir)
	@for plugin in $(PLUGINS); do \
		echo "  INSTALL $(DESTDIR)$(plugindir)/$$plugin";	\
		$(STRIPINST) $$plugin $(DESTDIR)$(plugindir)/$$plugin;	\
	done

uninstall:
	-@for plugin in $(PLUGINS); do \
		echo "  RM      $(DESTDIR)$(plugindir)/$$plugin";	\
		rm $(DESTDIR)$(plugindir)/$$plugin 2>/dev/null; 	\
	done
	-@rmdir $(DESTDIR)$(plugindir) 2>/dev/null

ifneq ($(MAKECMDGOALS),clean)
 -include $(DEPS)
endif
