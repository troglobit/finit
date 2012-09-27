# finit - The Improved fast init
#
# Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
# Copyright (c) 2008-2012  Joachim Nilsson <troglobit@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

.PHONY: all romfs clean

# Top directory for building complete system, fall back to this directory
ROOTDIR    ?= $(shell pwd)

# Installation paths, always prepended with DESTDIR if set
prefix     ?= /usr/local
sysconfdir ?= /etc
datadir     = $(prefix)/share/doc/finit
mandir      = $(prefix)/share/man/man8

# Plugin directory, fall back to this directory if unset in environment
PLUGIN_DIR ?= $(prefix)/lib/finit/plugins
# The initctl FIFO, should probably be in /run, not /dev
FINIT_FIFO ?= /dev/initctl
FINIT_CONF ?= $(sysconfdir)/finit.conf
FINIT_RCSD ?= $(sysconfdir)/finit.d

#VERSION    ?= $(shell git tag -l | tail -1)
VERSION    ?= 1.2
EXEC        = finit
PKG         = $(EXEC)-$(VERSION)
ARCHIVE     = $(PKG).tar.xz
OBJS        = finit.o conf.o helpers.o sig.o svc.o plugin.o
OBJS       += strlcpy.o
SRCS        = $(OBJS:.o=.c)
DEPS        = $(addprefix .,$(SRCS:.c=.d))
CFLAGS     += -W -Wall -Werror -Os
# Disable annoying gcc warning for "warn_unused_result", see GIT 37af997
CPPFLAGS   += -U_FORTIFY_SOURCE
CPPFLAGS   += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE
CPPFLAGS   += -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
CPPFLAGS   += -DFINIT_FIFO=\"$(FINIT_FIFO)\" -DFINIT_CONF=\"$(FINIT_CONF)\"
CPPFLAGS   += -DFINIT_RCSD=\"$(FINIT_RCSD)\" -DPLUGIN_PATH=\"$(PLUGIN_DIR)\"
LDFLAGS    += -rdynamic
LDLIBS     += -ldl

include common.mk
export PLUGIN_DIR ROOTDIR CPPFLAGS

all: Makefile $(EXEC)
	$(MAKE) -C plugins $@

$(OBJS): Makefile

$(EXEC): $(OBJS)

#	@ln -sf /sbin/finit /sbin/init
install: all
	@install -d $(DESTDIR)$(prefix)/sbin
	@install -d $(DESTDIR)$(sysconfdir)
	@install -d $(DESTDIR)$(datadir)
	@install -d $(DESTDIR)$(mandir)
	@for file in $(EXEC); do                                        \
		printf "  INSTALL $(DESTDIR)$(prefix)/sbin/$$file\n";   \
		install -m 0755 $$file $(DESTDIR)$(prefix)/sbin/$$file; \
	done
	$(MAKE) -C plugins $@

uninstall:
	-@for file in $(EXEC); do \
		printf "  REMOVE  $(DESTDIR)$(prefix)/sbin/$$file\n";   \
		rm $(DESTDIR)$(prefix)/sbin/$$file 2>/dev/null; \
	done
	$(MAKE) -C plugins $@

clean:
	-@$(RM) $(OBJS) $(DEPS) $(EXEC)
	$(MAKE) -C plugins $@

distclean: clean
	-@$(RM) $(JUNK)  unittest *.o
	$(MAKE) -C plugins $@

dist:
	@echo "Building xz tarball of $(PKG) in parent dir..."
	git archive --format=tar --prefix=$(PKG)/ $(VERSION) | xz >../$(ARCHIVE)
	@(cd ..; md5sum $(ARCHIVE) | tee $(ARCHIVE).md5)

# Include automatically generated rules, such as:
# uncgi.o: .../some/dir/uncgi.c /usr/include/stdio.h
# but don't bother during clean!
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
endif
