# Plugin based services architecture for finit -*-Makefile-*-
CFLAGS      = -fPIC -g -Os
CPPFLAGS   += -I..
LDFLAGS     = -shared 
PLUGINS    ?= initctl.so alsa-utils.so bootmisc.so dbus.so hwclock.so \
	      resolvconf.so urandom.so x11-common.so
DEPS        = $(addprefix .,$(PLUGINS:.so=.d))

include ../common.mk

all: $(PLUGINS)

clean:
	-@$(RM) $(PLUGINS) *.o

distclean: clean
	-@$(RM) $(JUNK) $(DEPS)

install: all
	@install -d $(DESTDIR)$(prefix)$(PLUGIN_DIR)
	@for plugin in $(PLUGINS); do \
		echo "  INSTALL $(DESTDIR)$(prefix)$(PLUGIN_DIR)/$$plugin"; \
		cp $$plugin $(DESTDIR)$(prefix)$(PLUGIN_DIR); \
	done

uninstall:
	-@for plugin in $(PLUGINS); do \
		echo "  RM      $(PLUGIN_DIR)/$$plugin"; \
		rm $(DESTDIR)$(prefix)$(PLUGIN_DIR)/$$plugin 2>/dev/null; \
	done
	-@rmdir $(DESTDIR)$(prefix)$(PLUGIN_DIR) 2>/dev/null

ifneq ($(MAKECMDGOALS),clean)
 -include $(DEPS)
endif
