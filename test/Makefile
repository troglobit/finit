ARCH ?= x86_64

busybox-ver = 1.31.0
busybox-bin = busybox-$(ARCH)

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
dirs = $(testroot)/dev \
              $(testroot)/etc \
              $(testroot)/lib \
              $(testroot)/proc \
              $(testroot)/sbin \
              $(testroot)/sys \
              $(testroot)/tmp \
              $(testroot)/usr/local/lib

$(testroot)/%: $(testroot)
	mkdir -p $@

$(busybox-bin):
	wget 'https://www.busybox.net/downloads/binaries/$(busybox-ver)-defconfig-multiarch-musl/$(busybox-bin)'
	md5sum -c $(busybox-bin).md5

$(testroot):
	mkdir $(testroot)

$(testroot_bin)/$(busybox-bin): $(testroot_bin) $(busybox-bin)
	cp $(busybox-bin) $(testroot_bin)/
	chmod +x $(testroot_bin)/$(busybox-bin)

$(testroot_bin)/%: $(testroot_bin)/$(busybox-bin)
	cd $(testroot_bin); \
	  ln -s $(busybox-bin) $(notdir $@)

$(testroot_bin)/chrootsetup.sh: chrootsetup.sh
	cp $< $@

$(testroot)/sbin/finit: ../src/finit $(testroot)/lib/ld-musl-x86_64.so.1 $(testroot)/usr/local/lib/libite.so.5 $(testroot)/usr/local/lib/libuev.so.2
	cp $< $@

$(testroot)/lib/ld-musl-x86_64.so.1:
	cp test_root.backup/lib/ld-musl-x86_64.so.1 $@

$(testroot)/usr/local/lib/libite.so.5:
	cp test_root.backup/usr/local/lib/libite.so.5 $@

$(testroot)/usr/local/lib/libuev.so.2:
	cp test_root.backup/usr/local/lib/libuev.so.2 $@

.PHONY: test
test: $(binaries) $(testroot_bin)/chrootsetup.sh $(dirs) $(testroot)/sbin/finit
	./runtest.sh ./test_start_stop_service/test.sh

.PHONY: clean
clean:
	rm -rf $(testroot)
	rm -f $(busybox-bin)
