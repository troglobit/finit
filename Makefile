# Finit - Fast /sbin/init replacement w/ I/O, hook & service plugins
#
# Copyright (c) 2008-2010  Claudio Matsuoka <cmatsuoka@gmail.com>
# Copyright (c) 2008-2015  Joachim Nilsson <troglobit@gmail.com>
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

#VERSION    ?= $(shell git tag -l | tail -1)
VERSION    ?= 1.13-dev
NAME        = finit
PKG         = $(NAME)-$(VERSION)
DEV         = $(NAME)-dev
ARCHTOOL    = `which git-archive-all`
ARCHIVE     = $(PKG).tar
ARCHIVEZ    = ../$(ARCHIVE).xz
EXEC        = finit initctl reboot
HEADERS     = plugin.h svc.h inetd.h helpers.h queue.h
DISTFILES   = LICENSE README CHANGELOG finit.conf services
OBJS        = finit.o api.o client.o conf.o exec.o helpers.o pid.o sig.o \
	      svc.o service.o plugin.o tty.o inetd.o

TOPDIR      = $(shell pwd)
-include config.mk

# Figure out source and dependency files
SRCS        = $(OBJS:.o=.c)
DEPS        = $(SRCS:.c=.d)

CFLAGS     += -W -Wall -Werror
# Disable annoying gcc warning for "warn_unused_result", see GIT 37af997
CPPFLAGS   += -U_FORTIFY_SOURCE
CPPFLAGS   += -I$(TOPDIR)
CPPFLAGS   += -D_XOPEN_SOURCE=600 -D_BSD_SOURCE -D_GNU_SOURCE -D_DEFAULT_SOURCE
CPPFLAGS   += -DVERSION=\"$(VERSION)\" -DWHOAMI=\"`whoami`@`hostname`\"
LDFLAGS    += -L$(TOPDIR)/libite -L$(TOPDIR)/libuev
DEPLIBS     = libite/libite.a libuev/libuev.a
LDLIBS     += -lite -luev

include common.mk


all: config.h $(EXEC)
	+$(MAKE) -C plugins $@

$(DEPLIBS): Makefile
	+$(MAKE) -C `dirname $@` all

$(OBJS): Makefile

config.h: configure
	@echo "Please run configure script first."
	@exit 1

finit: $(OBJS) $(DEPLIBS)

initctl: initctl.o svc.o helpers.o $(DEPLIBS)

reboot: reboot.o $(DEPLIBS)

install-exec: all
	@$(INSTALL) -d $(DESTDIR)$(FINIT_RCSD)
	@$(INSTALL) -d $(DESTDIR)$(sbindir)
	@for file in $(EXEC); do                                        \
		printf "  INSTALL $(DESTDIR)$(sbindir)/$$file\n";   	\
		$(STRIPINST) $$file $(DESTDIR)$(sbindir)/$$file; 	\
	done
	printf "  INSTALL $(DESTDIR)$(sbindir)/telinit\n"
	@ln -sf  finit $(DESTDIR)$(sbindir)/telinit
	$(MAKE) -C libite  install-exec
	$(MAKE) -C plugins install

install-data:
	@$(INSTALL) -d $(DESTDIR)$(datadir)
	@$(INSTALL) -d $(DESTDIR)$(mandir)
	@for file in $(DISTFILES); do	                                \
		printf "  INSTALL $(DESTDIR)$(datadir)/$$file\n";	\
		$(INSTALL) -m 0644 $$file $(DESTDIR)$(datadir)/$$file;	\
	done

install-dev:
	@$(INSTALL) -d $(DESTDIR)$(incdir)
	@for file in $(HEADERS); do	                                \
		printf "  INSTALL $(DESTDIR)$(incdir)/$$file\n";	\
		$(INSTALL) -m 0644 $$file $(DESTDIR)$(incdir)/$$file;	\
	done
	$(MAKE) -C libite install-dev
	$(MAKE) -C libuev install-dev

install: install-exec install-data install-dev

uninstall-exec:
	-@for file in $(EXEC); do 					\
		printf "  REMOVE  $(DESTDIR)$(sbindir)/$$file\n";   	\
		rm $(DESTDIR)$(sbindir)/$$file 2>/dev/null; 		\
	done
	-@rm  $(DESTDIR)$(sbindir)/initctl
	-@rmdir $(DESTDIR)$(sbindir) 2>/dev/null
	-@rmdir $(DESTDIR)$(FINIT_RCSD) 2>/dev/null
	$(MAKE) -C libite  uninstall
	$(MAKE) -C plugins uninstall

uninstall-data:
	@for file in $(DISTFILES); do	                                \
		printf "  REMOVE  $(DESTDIR)$(datadir)/$$file\n";	\
		rm $(DESTDIR)$(datadir)/$$file 2>/dev/null;		\
	done
	-@rmdir $(DESTDIR)$(mandir)
	-@rmdir $(DESTDIR)$(datadir)

uninstall-dev:
	-@$(RM) -rf $(DESTDIR)$(incdir)

uninstall: uninstall-exec uninstall-data uninstall-dev

clean:
	+$(MAKE) -C plugins $@
	+$(MAKE) -C libite  $@
	+$(MAKE) -C libuev  $@
	-@$(RM) $(OBJS) $(DEPS) $(EXEC)

distclean: clean
	+$(MAKE) -C plugins $@
	+$(MAKE) -C libite  $@
	+$(MAKE) -C libuev  $@
	-@$(RM) $(JUNK) config.mk config.h unittest *.o *.html

check:
	$(CHECK) *.c plugins/*.c libite/*.c

dist:
	@if [ x"$(ARCHTOOL)" = x"" ]; then \
		echo "Missing git-archive-all from https://github.com/Kentzo/git-archive-all"; \
		exit 1; \
	fi
	@if [ -e $(ARCHIVEZ) ]; then \
		echo "Distribution already exists."; \
		exit 1; \
	fi
	@echo "Building xz tarball of $(PKG) in parent dir..."
	@$(ARCHTOOL) ../$(ARCHIVE)
	@xz ../$(ARCHIVE)
	@md5sum $(ARCHIVEZ) | tee $(ARCHIVEZ).md5

dev: distclean
	@echo "Building unstable xz $(DEV) in parent dir..."
	-@$(RM) -f ../$(DEV).tar.xz*
	@(dir=`mktemp -d`; mkdir $$dir/$(DEV); cp -a . $$dir/$(DEV); \
	  cd $$dir; tar --exclude=.git --exclude=contrib             \
                        -c -J -f $(DEV).tar.xz $(DEV);               \
	  cd - >/dev/null; mv $$dir/$(DEV).tar.xz ../; cd ..;        \
	  rm -rf $$dir; md5sum $(DEV).tar.xz | tee $(DEV).tar.xz.md5)

ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
