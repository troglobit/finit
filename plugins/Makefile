# Plugin based services architecture for finit
-include ../config.mk

TOPDIR     ?= $(shell pwd)
CFLAGS     += -fPIC
LDFLAGS    := $(filter-out -rdynamic, $(LDFLAGS))
LDFLAGS    += -shared
ifneq ($(STATIC), 1)
PLUGINS    ?= initctl.so alsa-utils.so bootmisc.so dbus.so hwclock.so \
	      resolvconf.so urandom.so x11-common.so tty.so time.so
DEPS       := $(PLUGINS:.so=.d)
endif

include ../common.mk

all: $(PLUGINS)

clean:
	-@$(RM) $(PLUGINS)

distclean:
	-@$(RM) *.o *.so $(JUNK) $(DEPS)

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

ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
