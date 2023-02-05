#!/bin/sh
# Verify that a service can restart itself, issue #280
set -eu

TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
	run "mkdir -p /etc/default"
}

test_teardown()
{
	say "Test done $(date)"

	say "Running test teardown."
	run "rm -f $FINIT_CONF"
}

check_restarts()
{
	assert "serv restarts" "$(texec cat "$1" | awk '{print $1;}')" -ge "$2"
}


# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

say "Add stanza to $FINIT_CONF"
run "echo 'service serv -np -r serv -- Restart self' > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

sleep 2
run "initctl status serv"
run "ps"

sleep 2
run "initctl status serv"
run "ps"

say 'Pending restarts by itself ...'
retry 'check_restarts /tmp/serv-restart.cnt 3' 20 1

return 0
