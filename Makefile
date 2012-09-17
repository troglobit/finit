# finit - The Improved fast init
#
# Copyright (c) 2008 Claudio Matsuoka
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

VERSION     = 1.0-pre
PKG	    = finit-$(VERSION)
EXEC        = finit
OBJS        = finit.o helpers.o initctl.o signal.o
SRCS        = $(OBJS:.o=.c)
DEPS        = $(addprefix .,$(SRCS:.c=.d))
CPPFLAGS   += -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
CPPFLAGS   += -DLISTEN_INITCTL -DBUILTIN_RUNPARTS
CPPFLAGS   += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE
# Disable annoying gcc warning for "warn_unused_result", see GIT 37af997
CPPFLAGS   += -U_FORTIFY_SOURCE
CPPFLAGS   += -I$(ROOTDIR)/$(CONFIG_LINUXDIR)/include/
CFLAGS     += -W -Wall -Werror -Os

prefix     ?= /usr/local
sysconfdir ?= /etc
datadir     = $(prefix)/share/doc/finit
mandir      = $(prefix)/share/man/man8

include common.mk

all: Makefile $(EXEC)

$(OBJS): Makefile

finit: $(OBJS)

install: all
	@install -d $(DESTDIR)$(prefix)/sbin
	@install -d $(DESTDIR)$(sysconfdir)
	@install -d $(DESTDIR)$(datadir)
	@install -d $(DESTDIR)$(mandir)
	@for file in $(EXEC); do \
		install -m 0755 $$file $(DESTDIR)$(prefix)/sbin/$$file; \
	done
	@ln -sf /sbin/finit /sbin/init

uninstall:
	-@for file in $(EXEC); do \
		$(RM) $(DESTDIR)$(prefix)/sbin/$$file; \
	done
	-@$(RM) /sbin/finit /sbin/init

clean:
	-@$(RM) $(OBJS) $(DEPS) $(EXEC)

distclean: clean
	-@$(RM) $(JUNK)  unittest *.o

# Include automatically generated rules, such as:
# uncgi.o: .../some/dir/uncgi.c /usr/include/stdio.h
# but don't bother during clean!
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
endif
