# Finit libite -- Light-weight utility functions and C-library extensions
# 
# Copyright (c) 2008-2014  Joachim Nilsson <troglobit@gmail.com>
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

.EXPORT_ALL_VARIABLES:
.PHONY: all install clean

include  ../common.mk

ifneq ($(STATIC), 1)
CFLAGS     += -fPIC
endif
HEADERS     = lite.h
OBJS       := copyfile.o dir.o fexist.o fisdir.o fmode.o rsync.o
OBJS       += strlcpy.o strlcat.o strtonum.o
DEPS       := $(addprefix ., $(OBJS:.o=.d))

VER         = 0
LIBNAME     = libite
SOLIB       = $(LIBNAME).so.$(VER)
SYMLIB      = $(LIBNAME).so
STATICLIB   = $(LIBNAME).a
ifeq ($(STATIC), 1)
TARGET      = $(STATICLIB)
else
TARGET      = $(STATICLIB) $(SOLIB)
endif

CHECK_FLAGS = $(CFLAGS) $(CPPFLAGS)

all: $(TARGET)

$(OBJS): Makefile

$(SOLIB): $(OBJS)
	@printf "  LINK    %s\n" $@
	@$(CC) $(LDFLAGS) -shared $(OBJS) -lrt -lcrypt -Wl,-soname,$@ -o $@

$(STATICLIB): Makefile $(OBJS)
	@printf "  ARCHIVE $@\n"
	@$(AR) $(ARFLAGS) $@ $(OBJS)

install-exec: all
	@install -d $(DESTDIR)$(libdir)
	@printf "  INSTALL $(DESTDIR)$(libdir)/$(SOLIB)\n"
	@$(STRIPINST)  $(SOLIB) $(DESTDIR)$(libdir)/$(SOLIB)
	@$(STRIPINST)  $(STATICLIB) $(DESTDIR)$(libdir)/$(STATICLIB)
	@ln -sf $(SOLIB) $(DESTDIR)$(libdir)/$(SYMLIB)

install-dev:
	@for file in $(HEADERS); do	                                \
		printf "  INSTALL $(DESTDIR)$(incdir)/$$file\n";	\
		$(INSTALL) -m 0644 $$file $(DESTDIR)$(incdir)/$$file;	\
	done

install: install-exec install-dev

uninstall:
	-@$(RM) $(DESTDIR)$(libdir)/$(SOLIB)
	-@$(RM) $(DESTDIR)$(libdir)/$(SYMLIB)
	-@$(RM) $(DESTDIR)$(libdir)/$(STATICLIB)
	-@for file in $(HEADERS); do			\
		$(RM) $(DESTDIR)$(incdir)/$$file;	\
	done

clean: uninstall
	-@$(RM) $(OBJS) $(DEPS) $(TARGET) $(VERLIB) $(STATICLIB)

distclean: clean
	-@$(RM) $(JUNK) unittest *.o .*.d *.unittest

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
endif
