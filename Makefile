# Finit - Extremely fast /sbin/init replacement w/ I/O, hook & service plugins
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

.PHONY: all install clean distclean dist		\
	install-exec install-data install-dev		\
	uninstall-exec uninstall-data uninstall-dev

# Top directory for building complete system, fall back to this directory
ROOTDIR    ?= $(shell pwd)

#VERSION    ?= $(shell git tag -l | tail -1)
VERSION    ?= 1.3
EXEC        = finit
PKG         = $(EXEC)-$(VERSION)
ARCHIVE     = $(PKG).tar.xz
HEADERS     = plugin.h svc.h helpers.h
OBJS        = finit.o conf.o helpers.o sig.o svc.o plugin.o
OBJS       += strlcpy.o
SRCS        = $(OBJS:.o=.c)
DEPS        = $(addprefix .,$(SRCS:.c=.d))

# Installation paths, always prepended with DESTDIR if set
prefix     ?= /usr/local
sysconfdir ?= /etc
incdir      = $(prefix)/include/finit
sbindir     = $(prefix)/sbin
datadir     = $(prefix)/share/doc/finit
mandir      = $(prefix)/share/man/man8

# Plugin directory, fall back to this directory if unset in environment
PLUGIN_DIR ?= $(prefix)/lib/finit/plugins
# The initctl FIFO, should probably be in /run, not /dev
FINIT_FIFO ?= /dev/initctl
FINIT_CONF ?= $(sysconfdir)/finit.conf
FINIT_RCSD ?= $(sysconfdir)/finit.d

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
install-exec: all
	@install -d $(DESTDIR)$(prefix)/sbin
	@install -d $(DESTDIR)$(sysconfdir)
	@install -d $(DESTDIR)$(sbindir)
	@for file in $(EXEC); do                                        \
		printf "  INSTALL $(DESTDIR)$(sbindir)/$$file\n";   	\
		install -m 0755 $$file $(DESTDIR)$(sbindir)/$$file; 	\
	done
	$(MAKE) -C plugins all

install-data:
	@install -d $(DESTDIR)$(datadir)
	@install -d $(DESTDIR)$(mandir)

install-dev:
	@install -d $(DESTDIR)$(incdir)
	@for file in $(HEADERS); do	                                \
		printf "  INSTALL $(DESTDIR)$(incdir)/$$file\n";	\
		install -m 0644 $$file $(DESTDIR)$(incdir)/$$file;	\
	done

install: install-exec install-data install-dev

uninstall-exec:
	-@for file in $(EXEC); do 					\
		printf "  REMOVE  $(DESTDIR)$(sbindir)/$$file\n";   	\
		rm $(DESTDIR)$(sbindir)/$$file 2>/dev/null; 		\
	done
	$(MAKE) -C plugins uninstall

uninstall-data:

uninstall-dev:
	-@for file in $(HEADERS); do 					\
		printf "  REMOVE  $(DESTDIR)$(incdir)/$$file\n";	\
		rm $(DESTDIR)$(incdir)/$$file 2>/dev/null; 		\
	done

uninstall: uninstall-exec uninstall-data uninstall-dev

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
