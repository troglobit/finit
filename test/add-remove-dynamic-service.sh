#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"

	say "Running test teardown."
	run "rm -f $FINIT_CONF"
}

say "Test start $(date)"

say "Add a dynamic service in $FINIT_CONF"
run "echo 'service [2345] kill:20 log service.sh -- Dyn service' > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.conf'
run "echo > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 0 service.sh'
