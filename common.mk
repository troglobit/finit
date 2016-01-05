# Top directory for building complete system, fall back to this directory
ROOTDIR    ?= $(TOPDIR)

# Strip binaries unless user explicitly disables it
STRIPARGS ?= -s --strip-program=$(CROSS)strip -m 0755

# Some junk files we always want to be removed when doing a make clean.
JUNK        = *~ *.bak *.map .*.d *.d DEADJOE semantic.cache *.gdb *.elf core core.*
MAKE       := @$(MAKE)
MAKEFLAGS   = --no-print-directory --silent
CHECK      := cppcheck $(CPPFLAGS) --quiet --enable=all
INSTALL    := install --backup=off
STRIPINST  := $(INSTALL) $(STRIPARGS)
ARFLAGS    := crus

export libdir plugindir incdir ROOTDIR CPPFLAGS LDFLAGS LDLIBS STATIC


# Override default implicit rules
%.o: %.c
	@printf "  CC      $(subst $(ROOTDIR)/,,$(shell pwd)/$@)\n"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -MMD -MP -o $@ $<

%: %.o
	@printf "  LINK    $(subst $(ROOTDIR)/,,$(shell pwd)/$@)\n"
	@$(CC) $(CFLAGS) $(LDFLAGS) -Wl,-Map,$@.map -o $@ $^ $(LDLIBS$(LDLIBS-$(@)))

%.so: %.o
	@printf "  PLUGIN  $(subst $(ROOTDIR)/,,$(shell pwd)/$@)\n"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
