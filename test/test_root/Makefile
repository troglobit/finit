.PHONY: clean check

ARCH ?= x86_64

FINITBIN   ?= ../../src/finit
INITCTLBIN ?= ../../src/initctl

BBVER ?= 1.31.0
BBBIN  = busybox-$(ARCH)
BBURL ?= https://www.busybox.net/downloads/binaries/$(BBVER)-defconfig-multiarch-musl/$(BBBIN)

binaries = ./bin/cat \
           ./bin/cp \
           ./bin/date \
           ./bin/echo \
           ./bin/env \
           ./bin/find \
           ./bin/kill \
           ./bin/ls \
           ./bin/mkdir \
           ./bin/mkfifo \
           ./bin/mknod \
           ./bin/mount \
           ./bin/printf \
           ./bin/pgrep \
           ./bin/ps \
           ./bin/rm \
           ./bin/sh \
           ./bin/sleep \
           ./bin/top \
           ./bin/touch
dirs = ./dev \
       ./etc \
       ./proc \
       ./sbin \
       ./sys \
       ./test_assets \
       ./tmp \
       ./etc

_libs_src = $(shell ldd $(FINITBIN) | grep -Eo '/[^ ]+')
libs = $(foreach path,$(_libs_src),.$(path))

all: $(binaries) $(dirs) ./sbin/finit ./bin/initctl

$(dirs):
	mkdir -p $@

./bin/$(BBBIN):
	wget -O $@ $(BBURL)
	chmod +x ./bin/$(BBBIN)
	md5sum -c $(BBBIN).md5

$(binaries): ./bin/$(BBBIN)
	cd ./bin; \
	  rm -f $(notdir $@); \
	  ln -s $(BBBIN) $(notdir $@)

./sbin/finit: $(FINITBIN) $(libs)
	cp $< $@

./bin/initctl: $(INITCTLBIN) $(libs)
	cp $< $@

$(libs):
	mkdir -p $(dir $@)
	cp /$@ $@

clean:
	rm -f ./bin/$(BBBIN)
	rm -f ./bin/initctl
	rm -rf $(binaries)
	rm -rf ./dev
	rm -rf ./etc
	rm -rf ./lib64
	rm -rf ./proc
	rm -rf ./sbin
	rm -rf ./sys
	rm -rf ./test_assets
	rm -rf ./tmp
	rm -rf ./usr
	rm -rf ./var
