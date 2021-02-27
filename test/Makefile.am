EXTRA_DIST		 = tenv/lib.sh tenv/exec.sh tenv/start.sh
EXTRA_DIST		+= tenv/root.mk tenv/busybox-x86_64.md5
EXTRA_DIST		+= tenv/chrootsetup.sh
EXTRA_DIST		+= setup-root.sh
EXTRA_DIST		+= common/service.conf common/service.sh
EXTRA_DIST		+= checkself.sh
EXTRA_DIST		+= add-remove-dynamic-service.sh
EXTRA_DIST		+= add-remove-dynamic-service-sub-config.sh
EXTRA_DIST		+= start-stop-service.sh
EXTRA_DIST		+= start-stop-service-sub-config.sh

AM_TESTS_ENVIRONMENT	 = TENV_ROOT='$(abs_builddir)/tenv-root/';
AM_TESTS_ENVIRONMENT	+= export TENV_ROOT;
AM_TESTS_ENVIRONMENT	+= export LD_LIBRARY_PATH;
AM_TESTS_ENVIRONMENT	+= top_builddir=$(top_builddir);
AM_TESTS_ENVIRONMENT	+= export top_builddir;

checkself.sh:
	echo '#!/bin/sh' > $@
	echo shellcheck $(addprefix '"$$srcdir"'/,$(filter %.sh,$(filter-out $@, $(EXTRA_DIST)))) >> $@
	chmod +x $@

TEST_EXTENSIONS		 = .sh
TESTS			 = checkself.sh
TESTS			+= setup-root.sh
TESTS			+= add-remove-dynamic-service.sh
TESTS			+= add-remove-dynamic-service-sub-config.sh
TESTS			+= start-stop-service.sh
TESTS			+= start-stop-service-sub-config.sh

clean-local:
	-rm -rf $(builddir)/tenv-root/
	-rm -f checkself.sh
