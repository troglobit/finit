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
	texec rm -f /test_assets/fserv
}

say "Test start $(date)"

cp "$TEST_DIR"/common/fserv "$TENV_ROOT"/test_assets/

say "Add service stanza in $FINIT_CONF"
texec sh -c "echo 'service [2345] pid:!/run/fserv.pid /test_assets/fserv -- Forking service' > $FINIT_CONF"

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
#texec sh -c "initctl status fserv"

retry 'assert_num_children 1 fserv'

say 'Stop the service'
texec sh -c "initctl stop fserv"

retry 'assert_num_children 0 fserv'

say 'Start the service again'
texec sh -c "initctl start fserv"

retry 'assert_num_children 1 fserv'
