.PHONY: clean check test-check

ARCH ?= x86_64

FINITBIN ?= ../src/finit

BBVER ?= 1.31.0
BBBIN = busybox-$(ARCH)
BBURL ?= https://www.busybox.net/downloads/binaries/$(BBVER)-defconfig-multiarch-musl/$(BBBIN)

testroot = test_root
testroot_bin = $(testroot)/bin
binaries = $(testroot_bin)/cat \
           $(testroot_bin)/cp \
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
       $(testroot)/proc \
       $(testroot)/sbin \
       $(testroot)/sys \
       $(testroot)/tmp

_libs_src = $(shell ldd $(FINITBIN) | grep -Eo '/[^ ]+')
libs = $(foreach path,$(_libs_src),$(testroot)$(path))

all: check

$(dirs):
	mkdir -p $@

$(BBBIN):
	wget -O $@ $(BBURL)
	md5sum -c $@.md5

$(testroot):
	mkdir $(testroot)

$(testroot_bin)/$(BBBIN): $(BBBIN)
	mkdir -p $(testroot_bin)
	cp $(BBBIN) $(testroot_bin)/
	chmod +x $(testroot_bin)/$(BBBIN)

$(binaries): $(testroot_bin)/$(BBBIN)
	cd $(testroot_bin); \
	  ln -s $(BBBIN) $(notdir $@)

$(testroot_bin)/chrootsetup.sh: chrootsetup.sh
	cp $< $@

$(testroot)/sbin/finit: $(FINITBIN) $(libs)
	cp $< $@

$(libs):
	mkdir -p $(dir $@)
	cp $(shell echo $@ | sed s/^$(testroot)//) $@

check: $(binaries) $(testroot_bin)/chrootsetup.sh $(dirs) $(testroot)/sbin/finit
	./runtest.sh test_start_stop_service/test.sh

test-check:
	shellcheck $(shell find . -name '*.sh')

clean:
	rm -rf $(testroot)
	rm -f $(BBBIN)
