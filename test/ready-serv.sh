#!/bin/sh
# Verify ready:script for pid/systemd/s6 style services
set -eu

TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
}

test_teardown()
{
	say "Test done $(date)"

	texec rm -f "$FINIT_CONF"
}

test_one()
{
	service=$1
	file=/tmp/ready

	say "Add service stanza '$service' to $FINIT_CONF ..."
	texec sh -c "echo '$service' > $FINIT_CONF"

	say 'Reload Finit'
	texec sh -c "initctl reload"

	retry 'assert_num_children 1 serv'
	assert_file_contains "$file" "READY"
	texec sh -c "rm -f $file"

	say 'Stop the service'
	texec sh -c "initctl stop serv"
	retry 'assert_num_children 0 serv'

	say "Done, drop service from $FINIT_CONF ..."
	texec sh -c "rm $FINIT_CONF"
	texec sh -c "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

#texec sh -c "initctl debug"

test_one "service                ready:/bin/ready.sh serv -np       -- Native style notification"
test_one "service notify:s6      ready:/bin/ready.sh serv -np -N %n -- s6 style notification"
test_one "service notify:systemd ready:/bin/ready.sh serv -np       -- systemd style notification"
