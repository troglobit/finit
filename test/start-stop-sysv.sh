#!/bin/sh
# shellcheck disable=2034 source=/dev/null

set -eu

STANZA1="sysv [2345] pid:!/run/service.pid name:service.sh log:stdout	\
		/etc/init.d/S01-service.sh -- SysV test service"
STANZA2="sysv [2345] pid:!/run/service.pid name:service.sh log:stdout	\
		reload:'/etc/init.d/S01-service.sh reload'		\
		stop:'/etc/init.d/S01-service.sh stop'			\
		/etc/init.d/S01-service.sh -- SysV test service"
STANZA3="sysv [2345] pid:!/run/service.pid name:service.sh log:stdout	\
		reload:'kill -HUP \\\$MAINPID'				\
		stop:'kill -TERM \\\$MAINPID'				\
		/etc/init.d/S01-service.sh -- SysV test service"
TEST_DIR=$(dirname "$0")
#DEBUG=1

test_teardown()
{
    assert "Running test teardown." "run rm -f $FINIT_RCSD/service.conf"
}

test_one()
{
    say "Add sysv stanza #1 in $FINIT_CONF"
    run "echo \"$1\" > $FINIT_RCSD/service.conf"

    say "Reload Finit"
    run "initctl reload"

    retry 'assert_num_children 1 service.sh'
    retry 'assert_cond service/service.sh/ready'

    sep 'Stop the sysv service'
    run "initctl stop service.sh"

    retry 'assert_num_children 0 service.sh'

    sep 'Start the sysv service again'
    run "initctl start service.sh"

    retry 'assert_num_children 1 service.sh'
    retry 'assert_cond service/service.sh/ready'

    sep 'Touch the sysv service'
    run "initctl touch service.conf"
    run "initctl reload"
    #run "initctl cond dump"

    retry 'assert_num_children 1 service.sh'
    retry 'assert_cond service/service.sh/ready'
}

. "$TEST_DIR/lib/setup.sh"

sep "―― 1) Basic stop/start/HUP sysv daemon"
test_one "$STANZA1"

sep "―― 2) Custom stop/reload script"
test_one "$STANZA2"

sep "―― 3) Custom stop/start/reload with \$MAINPID"
test_one "$STANZA3"

return 0
