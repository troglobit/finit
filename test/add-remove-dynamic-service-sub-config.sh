#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

test_teardown()
{
    say "Running test teardown."
    run "rm -f $FINIT_RCSD/service.conf"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

say "Add a dynamic service in $FINIT_RCSD/service.conf"
run "echo 'service [2345] kill:20 log service.sh -- Dyn serv' > $FINIT_RCSD/service.conf"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.d/service.conf'
run "echo > $FINIT_RCSD/service.conf"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 0 service.sh'
