#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

say "Test start $(date)"

say "Add sysv stanza in $FINIT_CONF"
run "echo 'sysv [2345] pid:!/run/service.pid name:service.sh /etc/init.d/S01-service.sh -- SysV test service' > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Stop the sysv service'
run "initctl stop service.sh"

retry 'assert_num_children 0 service.sh'

say 'Start the sysv service again'
run "initctl start service.sh"

retry 'assert_num_children 1 service.sh'

return 0
