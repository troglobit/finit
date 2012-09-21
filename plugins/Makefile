# Plugin based services architecture for finit -*-Makefile-*-
CFLAGS      = -fPIC -g -Os -I..
CPPFLAGS    = -W -Wall
LDFLAGS     = -shared 
PLUGINS     = alsa-utils.so bootmisc.so dbus.so hwclock.so resolvconf.so \
	      urandom.so x11-common.so
DEPS        = $(addprefix .,$(PLUGINS:.so=.d))

include ../common.mk

all: $(PLUGINS)

clean:
	$(RM) $(PLUGINS) *.o

distclean: clean
	$(RM) $(JUNK) $(DEPS)

install: all
	@for plugin in $(PLUGINS); do \
		echo "  INSTALL $(PLUGIN_DIR)/$$plugin"; \
		cp $$plugin $(PLUGIN_DIR); \
	done

uninstall:
	@for plugin in $(PLUGINS); do \
		echo "  RM      $(PLUGIN_DIR)/$$plugin"; \
		rm $(PLUGIN_DIR)/$$plugin; \
	done

ifneq ($(MAKECMDGOALS),clean)
 -include $(DEPS)
endif
