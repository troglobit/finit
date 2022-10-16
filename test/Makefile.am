SUBDIRS                  = common tenv

EXTRA_DIST		 = skel/bin/busybox-x86_64.sha256 skel/sbin/service.sh skel/etc/env  \
			   skel/bin/pre.sh skel/bin/post.sh skel/bin/ready.sh skel/bin/slay  \
			   skel/cdrom/.empty skel/dev/shm/.empty skel/dev/pts/.empty	     \
			   skel/etc/inittab skel/etc/hostname skel/etc/fstab 		     \
			   skel/etc/init.d/rcS skel/etc/init.d/rcK skel/tmp/.empty	     \
			   skel/etc/finit.d/.empty skel/sbin/task.sh 	                     \
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

EXTRA_DIST		+= setup-root.sh
EXTRA_DIST		+= add-remove-dynamic-service.sh
EXTRA_DIST		+= add-remove-dynamic-service-sub-config.sh
EXTRA_DIST		+= cond-start-task.sh
EXTRA_DIST		+= failing-sysv.sh
EXTRA_DIST		+= global-envs.sh
EXTRA_DIST		+= initctl-status-subset.sh
EXTRA_DIST		+= notify.sh
EXTRA_DIST		+= pre-post-serv.sh
EXTRA_DIST		+= ready-serv.sh
EXTRA_DIST		+= restart-self.sh
EXTRA_DIST		+= runlevel.sh
EXTRA_DIST		+= start-stop-service.sh
EXTRA_DIST		+= start-stop-service-sub-config.sh
EXTRA_DIST		+= start-kill-service.sh
EXTRA_DIST		+= start-stop-sysv.sh
EXTRA_DIST		+= start-stop-serv.sh
EXTRA_DIST		+= signal-service.sh

AM_TESTS_ENVIRONMENT	 = TENV_ROOT='$(abs_builddir)/tenv-root/';
AM_TESTS_ENVIRONMENT	+= export TENV_ROOT;
AM_TESTS_ENVIRONMENT	+= export LD_LIBRARY_PATH;
AM_TESTS_ENVIRONMENT	+= top_builddir=$(top_builddir);
AM_TESTS_ENVIRONMENT	+= export top_builddir;

checkself.sh:
	@echo '#!/bin/sh' > $@
	@echo shellcheck $(addprefix '"$$srcdir"'/,$(filter %.sh,$(filter-out $@, $(EXTRA_DIST)))) >> $@
	@chmod +x $@

TEST_EXTENSIONS		 = .sh
TESTS			 = checkself.sh
TESTS			+= add-remove-dynamic-service.sh
TESTS			+= add-remove-dynamic-service-sub-config.sh
TESTS			+= cond-start-task.sh
TESTS			+= failing-sysv.sh
TESTS			+= global-envs.sh
TESTS			+= initctl-status-subset.sh
TESTS			+= notify.sh
TESTS			+= pre-post-serv.sh
TESTS			+= ready-serv.sh
TESTS			+= restart-self.sh
TESTS			+= runlevel.sh
TESTS			+= start-stop-service.sh
TESTS			+= start-stop-service-sub-config.sh
TESTS			+= start-kill-service.sh
TESTS			+= start-stop-sysv.sh
TESTS			+= start-stop-serv.sh
TESTS			+= signal-service.sh

check-recursive: setup-chroot

.PHONY: setup-chroot
setup-chroot:
	@TENV_ROOT='$(abs_builddir)/tenv-root/' srcdir=$(srcdir) top_builddir=$(top_builddir) $(srcdir)/setup-root.sh

clean-local:
	-rm -rf $(builddir)/tenv-root/
	-rm -f checkself.sh
