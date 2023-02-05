#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"

	say "Running test teardown."
	run "rm -f $FINIT_RCSD/service.conf"
}

say "Test start $(date)"

say "Add service stanza in $FINIT_RCSD/service.conf"
run "echo 'service [2345] kill:20 log service.sh -- Subserv' > $FINIT_RCSD/service.conf"

say 'Reload Finit'
run "initctl reload"

sleep 2
run "ps"

retry 'assert_num_children 1 service.sh'

say 'Stop the service'
run "initctl stop service.sh"

retry 'assert_num_children 0 service.sh'

say 'Start the service again'
run "initctl start service.sh"

retry 'assert_num_children 1 service.sh'
