EXTRA_DIST		 = lib.sh testenv_exec.sh testenv_start.sh
EXTRA_DIST		+= test_root/Makefile test_root/busybox-x86_64.md5
EXTRA_DIST		+= test_root/bin/chrootsetup.sh
EXTRA_DIST		+= checkself.sh
EXTRA_DIST		+= setup_root.sh
EXTRA_DIST		+= teardown_root.sh
EXTRA_DIST		+= test_start_stop_service/test.sh
EXTRA_DIST		+= test_start_stop_service/service.sh

AM_TESTS_ENVIRONMENT	 = TESTS_ROOT='$(builddir)/test_root/';
AM_TESTS_ENVIRONMENT	+= export TESTS_ROOT;
AM_TESTS_ENVIRONMENT	+= export LD_LIBRARY_PATH;
AM_TESTS_ENVIRONMENT	+= PREFIX=$(prefix);
AM_TESTS_ENVIRONMENT	+= export PREFIX;

TESTS			 = checkself.sh
TESTS			+= setup_root.sh
TESTS			+= test_start_stop_service/test.sh
TESTS			+= teardown_root.sh
