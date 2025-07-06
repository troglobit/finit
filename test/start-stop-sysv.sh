#!/bin/sh
# shellcheck disable=2034 source=/dev/null

set -eu

STANZA="sysv [2345] pid:!/run/service.pid name:service.sh log:stdout \
		reload:'/etc/init.d/S01-service.sh reload'    \
		/etc/init.d/S01-service.sh -- SysV test service"
TEST_DIR=$(dirname "$0")
#DEBUG=1

test_teardown()
{
    sep "Test done $(date)"
    assert "Running test teardown." "run rm -f $FINIT_RCSD/service.conf"
}

. "$TEST_DIR/lib/setup.sh"

sep "Test start $(date)"

say "Add sysv stanza in $FINIT_CONF"
run "echo \"$STANZA\" > $FINIT_RCSD/service.conf"

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

return 0
