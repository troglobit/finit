EXTRA_DIST		 = lib.sh testenv_exec.sh testenv_start.sh
EXTRA_DIST		+= test-root/Makefile test-root/busybox-x86_64.md5
EXTRA_DIST		+= test-root/bin/chrootsetup.sh
EXTRA_DIST		+= setup-root.sh
EXTRA_DIST		+= common/service.conf common/service.sh
EXTRA_DIST		+= checkself.sh
EXTRA_DIST		+= add-remove-dynamic-service/test.sh
EXTRA_DIST		+= add-remove-dynamic-service-sub-config/test.sh
EXTRA_DIST		+= start-stop-service/test.sh
EXTRA_DIST		+= start-stop-service-sub-config/test.sh

AM_TESTS_ENVIRONMENT	 = TESTS_ROOT='$(abs_builddir)/test-root/';
AM_TESTS_ENVIRONMENT	+= export TESTS_ROOT;
AM_TESTS_ENVIRONMENT	+= export LD_LIBRARY_PATH;
AM_TESTS_ENVIRONMENT	+= top_srcdir=$(top_srcdir);
AM_TESTS_ENVIRONMENT	+= export top_srcdir;
AM_TESTS_ENVIRONMENT	+= top_builddir=$(top_builddir);
AM_TESTS_ENVIRONMENT	+= export top_builddir;

TESTS			 = checkself.sh
TESTS			+= setup-root.sh
TESTS			+= add-remove-dynamic-service/test.sh
TESTS			+= add-remove-dynamic-service-sub-config/test.sh
TESTS			+= start-stop-service/test.sh
TESTS			+= start-stop-service-sub-config/test.sh

clean-local:
	make -C test-root/ clean
