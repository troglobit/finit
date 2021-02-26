.PHONY: clean check

DEST		?= ./testenv-root
srcdir		?= ./

ARCH		?= x86_64

FINITBIN   ?= ./sbin/finit

BBVER ?= 1.31.0
BBBIN  = busybox-$(ARCH)
BBURL ?= https://www.busybox.net/downloads/binaries/$(BBVER)-defconfig-multiarch-musl/$(BBBIN)

binaries = $(DEST)/bin/cat \
	$(DEST)/bin/cp \
	$(DEST)/bin/date \
	$(DEST)/bin/echo \
	$(DEST)/bin/env \
	$(DEST)/bin/find \
	$(DEST)/bin/kill \
	$(DEST)/bin/ls \
	$(DEST)/bin/mkdir \
	$(DEST)/bin/mkfifo \
	$(DEST)/bin/mknod \
	$(DEST)/bin/mount \
	$(DEST)/bin/printf \
	$(DEST)/bin/pgrep \
	$(DEST)/bin/ps \
	$(DEST)/bin/rm \
	$(DEST)/bin/sh \
	$(DEST)/bin/sleep \
	$(DEST)/bin/top \
	$(DEST)/bin/touch

dirs = $(DEST)/bin \
	$(DEST)/dev \
	$(DEST)/etc \
	$(DEST)/proc \
	$(DEST)/sbin \
	$(DEST)/sys \
	$(DEST)/test_assets \
	$(DEST)/tmp

_libs_src = $(shell ldd $(FINITBIN) | grep -Eo '/[^ ]+')
libs = $(foreach path,$(_libs_src),$(abspath $(DEST))$(path))

all: $(dirs) $(binaries) $(libs) $(DEST)/bin/chrootsetup.sh

$(dirs):
	mkdir -p $@

$(DEST)/bin/$(BBBIN):
	wget -O $@ $(BBURL)
	chmod +x $@
	# md5sum -c $(BBBIN).md5

$(DEST)/bin/chrootsetup.sh:
	cp $(srcdir)/$(notdir $@) $@

$(binaries): $(DEST)/bin/$(BBBIN)
	cd $(DEST)/bin; \
	  rm -f $(notdir $@); \
	  ln -s $(BBBIN) $(notdir $@)

$(libs):
	mkdir -p $(dir $@)
	cp $(patsubst $(abspath $(DEST))%,%,$@) $@
