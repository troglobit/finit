ARCH ?= x86_64

finit_bin = ../src/finit

busybox_ver = 1.31.0
busybox_bin = busybox-$(ARCH)

testroot = test_root
testroot_bin = $(testroot)/bin
binaries = $(testroot_bin)/cat \
           $(testroot_bin)/date \
           $(testroot_bin)/echo \
           $(testroot_bin)/kill \
           $(testroot_bin)/ls \
           $(testroot_bin)/mkdir \
           $(testroot_bin)/mkfifo \
           $(testroot_bin)/mknod \
           $(testroot_bin)/mount \
           $(testroot_bin)/printf \
           $(testroot_bin)/pgrep \
           $(testroot_bin)/ps \
           $(testroot_bin)/rm \
           $(testroot_bin)/sh \
           $(testroot_bin)/sleep \
           $(testroot_bin)/top \
           $(testroot_bin)/touch
dirs = $(testroot)/bin \
       $(testroot)/dev \
       $(testroot)/etc \
       $(testroot)/lib \
       $(testroot)/lib64 \
       $(testroot)/proc \
       $(testroot)/sbin \
       $(testroot)/sys \
       $(testroot)/tmp \
       $(testroot)/usr/lib

$(dirs):
	mkdir -p $@

$(busybox_bin):
	wget 'https://www.busybox.net/downloads/binaries/$(busybox_ver)-defconfig-multiarch-musl/$(busybox_bin)'
	md5sum -c $(busybox_bin).md5

$(testroot):
	mkdir $(testroot)

$(testroot_bin)/$(busybox_bin): $(busybox_bin)
	mkdir -p $(testroot_bin)
	cp $(busybox_bin) $(testroot_bin)/
	chmod +x $(testroot_bin)/$(busybox_bin)

$(binaries): $(testroot_bin)/$(busybox_bin)
	cd $(testroot_bin); \
	  ln -s $(busybox_bin) $(notdir $@)

$(testroot_bin)/chrootsetup.sh: chrootsetup.sh
	cp $< $@

libs_src = $(shell ldd $(finit_bin) | grep -v lib64 | grep '=>' | cut -d' ' -f3)
libs = $(foreach path,$(libs_src),$(testroot)/usr/lib/$(shell basename $(path)))
libs64_src = $(shell ldd $(finit_bin) | grep lib64 | grep '=>' | cut -d' ' -f3)
libs64 = $(foreach path,$(libs64_src),$(testroot)/lib64/$(shell basename $(path)))

$(testroot)/sbin/finit: $(finit_bin) $(libs) $(libs64)
	cp $< $@

.PHONY: test
test: $(binaries) $(testroot_bin)/chrootsetup.sh $(dirs) $(testroot)/sbin/finit
	./runtest.sh test_start_stop_service/test.sh

$(libs): $@
	cp $(shell ldd $(finit_bin) | cut -d' ' -f3 | grep $(notdir $@)) $@

$(libs64): $@
	cp $(shell ldd $(finit_bin) | cut -d' ' -f3 | grep $(notdir $@)) $@

.PHONY: lint
lint:
	shellcheck $(shell find . -name '*.sh')

.PHONY: clean
clean:
	rm -rf $(testroot)
	rm -f $(busybox_bin)
