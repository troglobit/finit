SUBDIRS                  = lib src

EXTRA_DIST		 = skel/bin/busybox-x86_64.sha256 skel/sbin/service.sh skel/etc/env  \
			   skel/bin/pre.sh skel/bin/post.sh skel/bin/ready.sh skel/bin/slay  \
			   skel/sbin/fail.sh			     			     \
			   skel/cdrom/.empty skel/dev/shm/.empty skel/dev/pts/.empty	     \
			   skel/etc/inittab skel/etc/hostname skel/etc/fstab 		     \
			   skel/etc/init.d/rcS skel/etc/init.d/rcK skel/tmp/.empty	     \
			   skel/etc/finit.d/.empty skel/sbin/task.sh skel/bin/crasher.sh     \
			   skel/etc/init.d/S01-service.sh skel/etc/init.d/S02-serv.sh	     \
			   skel/proc/.empty skel/root/.empty skel/run/.empty		     \
			   skel/sbin/chrootsetup.sh skel/srv/.empty skel/sys/.empty	     \
			   skel/usr/bin/.empty skel/usr/lib/.empty skel/usr/sbin/.empty	     \
			   skel/home/.empty skel/usr/games/.empty skel/usr/src/.empty	     \
			   skel/usr/share/.empty skel/usr/include/.empty		     \
			   skel/usr/local/games/.empty skel/usr/local/sbin/.empty	     \
			   skel/usr/local/share/.empty skel/usr/local/src/.empty	     \
			   skel/usr/local/lib/.empty skel/usr/local/bin/.empty		     \
			   skel/usr/local/etc/.empty skel/var/.empty

EXTRA_DIST		+= setup-sysroot.sh
EXTRA_DIST		+= add-remove-dynamic-service.sh
EXTRA_DIST		+= add-remove-dynamic-service-sub-config.sh
EXTRA_DIST		+= bootstrap-crash.sh
EXTRA_DIST		+= cond-start-task.sh
EXTRA_DIST		+= crashing.sh
EXTRA_DIST		+= depserv.sh
EXTRA_DIST		+= devmon.sh
EXTRA_DIST		+= failing-sysv.sh
EXTRA_DIST		+= global-envs.sh
EXTRA_DIST		+= initctl-status-subset.sh
EXTRA_DIST		+= notify.sh
EXTRA_DIST		+= pidfile.sh
EXTRA_DIST		+= pre-post-serv.sh
EXTRA_DIST		+= rclocal.sh
EXTRA_DIST		+= ready-serv.sh
EXTRA_DIST		+= restart-self.sh
EXTRA_DIST		+= runlevel.sh
EXTRA_DIST		+= run-restart-forever.sh
EXTRA_DIST		+= start-stop-service.sh
EXTRA_DIST		+= start-stop-service-sub-config.sh
EXTRA_DIST		+= start-kill-service.sh
EXTRA_DIST		+= start-kill-stop.sh
EXTRA_DIST		+= start-stop-sysv.sh
EXTRA_DIST		+= start-stop-serv.sh
EXTRA_DIST		+= signal-service.sh
EXTRA_DIST		+= testserv.sh

AM_TESTS_ENVIRONMENT	 = SYSROOT='$(abs_builddir)/sysroot/';
AM_TESTS_ENVIRONMENT	+= export SYSROOT;
AM_TESTS_ENVIRONMENT	+= export LD_LIBRARY_PATH;
AM_TESTS_ENVIRONMENT	+= top_builddir=$(top_builddir);
AM_TESTS_ENVIRONMENT	+= export top_builddir;

checkself.sh:
	@echo '#!/bin/sh' > $@
	@echo shellcheck $(addprefix '"$$srcdir"'/,$(filter %.sh,$(filter-out $@, $(TESTS)))) >> $@
	@chmod +x $@

TEST_EXTENSIONS		 = .sh
TESTS			 = checkself.sh
TESTS			+= add-remove-dynamic-service.sh
TESTS			+= add-remove-dynamic-service-sub-config.sh
TESTS			+= bootstrap-crash.sh
TESTS			+= cond-start-task.sh
TESTS			+= crashing.sh
TESTS			+= depserv.sh
TESTS			+= devmon.sh
TESTS			+= failing-sysv.sh
TESTS			+= global-envs.sh
TESTS			+= initctl-status-subset.sh
TESTS			+= notify.sh
TESTS			+= pidfile.sh
TESTS			+= pre-post-serv.sh
TESTS			+= rclocal.sh
TESTS			+= ready-serv.sh
TESTS			+= restart-self.sh
TESTS			+= runlevel.sh
TESTS			+= run-restart-forever.sh
TESTS			+= start-stop-service.sh
TESTS			+= start-stop-service-sub-config.sh
TESTS			+= start-kill-service.sh
TESTS			+= start-kill-stop.sh
TESTS			+= start-stop-sysv.sh
TESTS			+= start-stop-serv.sh
TESTS			+= signal-service.sh
if TESTSERV
TESTS			+= testserv.sh
endif

check-recursive: setup-chroot

.PHONY: setup-chroot
setup-chroot:
	@SYSROOT='$(abs_builddir)/sysroot/' srcdir=$(srcdir) top_builddir=$(top_builddir) $(srcdir)/setup-sysroot.sh

clean-local:
	-rm -rf $(builddir)/sysroot/
	-rm -f checkself.sh
