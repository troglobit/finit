#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
	texec rm -f /test_assets/serv
}

say "Test start $(date)"

cp "$TENV_ROOT"/../common/serv "$TENV_ROOT"/test_assets/

say "Add service stanza in $FINIT_CONF"
texec sh -c "echo 'service [2345] pid:!/run/serv.pid /test_assets/serv -- Forking service' > $FINIT_CONF"

say 'Reload Finit'
#texec sh -c "initctl debug"
texec sh -c "initctl reload"
#texec sh -c "initctl status"
#texec sh -c "ps"

# Wait for process to fork, create its PID file so Finit can register
# its new PID -- this is the major difference from regular services.
sleep 2

#texec sh -c "initctl status"
#texec sh -c "ps"
#texec sh -c "initctl status serv"

retry 'assert_num_children 1 serv'

say 'Stop the service'
texec sh -c "initctl stop serv"

retry 'assert_num_children 0 serv'

say 'Start the service again'
texec sh -c "initctl start serv"

retry 'assert_num_children 1 serv'
