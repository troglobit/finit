# Root file system for test environment.
#
# Copyright (c) 2021  Jacques de Laval <jacques@de-laval.se>
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

srcdir         ?= ../
SKEL           ?= $(srcdir)/skel
DEST           ?= ../sysroot

CACHE          ?= ~/.cache
ARCH           ?= x86_64

FINITBIN       ?= ./sbin/finit

BBVER          ?= 1_35_0
BBBIN           = busybox-$(ARCH)
BBHOME         ?= https://github.com/troglobit/busybox-builder/releases/download
BBURL          ?= $(BBHOME)/$(BBVER)/$(BBBIN)

_libs_src       = $(shell ldd $(FINITBIN) | grep -Eo '/[^ ]+')
libs            = $(foreach path,$(_libs_src),$(abspath $(DEST))$(path))

all: $(libs) $(DEST)/bin/$(BBBIN)
	@(cd $(DEST);					\
	for prg in `./bin/$(BBBIN) --list-full`; do 	\
	    case $$prg in				\
	        usr/bin/* | usr/sbin/*)			\
		    ln -sf ../../bin/$(BBBIN) $$prg;;	\
                bin/* | sbin/*)				\
                    ln -sf ../bin/$(BBBIN) $$prg;;	\
                *)					\
                    ln -sf bin/$(BBBIN) $$prg;;		\
	    esac;					\
	done)

$(DEST)/bin/$(BBBIN).sha256:
	@mkdir -p $(DEST)
	@cp -a $(SKEL)/* $(DEST)/
	@chmod -R u+w $(DEST)/
	@find $(DEST) -name .empty -delete

$(DEST)/bin/$(BBBIN): $(DEST)/bin/$(BBBIN).sha256
	@(cd $(dir $@);								\
	if ! sha256sum --status -c $(BBBIN).sha256 2>/dev/null; then		\
		if [ -d $(CACHE) ]; then					\
			echo "Cannot find $(BBBIN), checking $(CACHE) ...";	\
			cd $(CACHE);						\
			cp -v $(DEST)/bin/$(BBBIN).sha256 .;			\
			if ! sha256sum --status -c $(BBBIN).sha256; then	\
				echo "No $(BBBIN), downloading $(BBURL) ...";	\
				wget -O $(BBBIN) $(BBURL);			\
			else							\
				echo "Found valid $(BBBIN) in cache!";		\
			fi;							\
			cp $(BBBIN) $@;						\
			cd $(dir $@);						\
		else								\
			echo "Cannot find $(BBBIN), downloading ...";		\
			wget -O $@ $(BBURL);					\
		fi;								\
		sha256sum -c $(BBBIN).sha256 || (rm $@; false);			\
	fi)
	@chmod +x $@

$(libs): $(DEST)/bin/$(BBBIN).sha256
	mkdir -p $(dir $@)
	cp $(patsubst $(abspath $(DEST))%,%,$@) $@
