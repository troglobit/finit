#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_RCSD/service.conf"
}

say "Test start $(date)"

say "Add service stanza in $FINIT_RCSD/service.conf"
texec sh -c "echo 'service [2345] kill:20 log service.sh -- Subserv' > $FINIT_RCSD/service.conf"

say 'Reload Finit'
texec sh -c "initctl reload"

sleep 2
texec sh -c "ps"

retry 'assert_num_children 1 service.sh'

say 'Stop the service'
texec sh -c "initctl stop service.sh"

retry 'assert_num_children 0 service.sh'

say 'Start the service again'
texec sh -c "initctl start service.sh"

retry 'assert_num_children 1 service.sh'
