# Plugin based services architecture for finit -*-Makefile-*-
CFLAGS      = -fPIC -g -Os -I..
CPPFLAGS    = -W -Wall
LDFLAGS     = -shared 
PLUGINS     = foo.so
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
