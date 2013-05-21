# Finit libite -- Light-weight utility functions and C-library extensions
# 
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

.EXPORT_ALL_VARIABLES:
.PHONY: all install clean

include  ../common.mk

CFLAGS     += -fPIC
HEADERS     = lite.h
OBJS       := copyfile.o dir.o fexist.o fisdir.o fmode.o rsync.o
OBJS       += strlcpy.o strlcat.o strtonum.o
DEPS       := $(addprefix ., $(OBJS:.o=.d))

VER         = 0
LIBNAME     = libite
TARGET      = $(LIBNAME).so
VERLIB      = $(TARGET).$(VER)
STATICLIB   = $(LIBNAME).a

CHECK_FLAGS = $(CFLAGS) $(CPPFLAGS)

all: $(TARGET)

$(OBJS): Makefile

$(TARGET): $(OBJS)
	@printf "  LINK    %s\n" $@
#	@$(AR) $(ARFLAGS) $(STATICLIB) $(OBJS)
	@$(CC) $(LDFLAGS) -shared $(OBJS) -lrt -lcrypt -Wl,-soname,$@ -o $@
	@ln -sf $@ $(VERLIB)

install-exec: all
	@install -d $(DESTDIR)$(libdir)
	@echo "  INSTALL $(DESTDIR)$(libdir)/$(TARGET)"
	@$(STRIPINST) $(TARGET) $(DESTDIR)$(libdir)/$(TARGET)
	@ln -sf $(TARGET) $(DESTDIR)$(libdir)/$(VERLIB)

install-dev:
	@for file in $(HEADERS); do	                                \
		printf "  INSTALL $(DESTDIR)$(incdir)/$$file\n";	\
		$(INSTALL) -m 0644 $$file $(DESTDIR)$(incdir)/$$file;	\
	done

install: install-dev install-exec

uninstall:
	-@$(RM) $(DESTDIR)$(libdir)/$(VERLIB)
	-@$(RM) $(DESTDIR)$(libdir)/$(TARGET)

clean: uninstall
	-@$(RM) $(OBJS) $(DEPS) $(TARGET) $(VERLIB) $(STATICLIB)

distclean: clean
	-@$(RM) $(JUNK) unittest *.o .*.d *.unittest

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPS)
endif
endif
