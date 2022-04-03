#!/bin/sh
set -eu

TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
	cp "$TENV_ROOT"/../common/serv "$TENV_ROOT"/test_assets/
}

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
	texec rm -f /test_assets/serv
}

test_one()
{
	pidfn=$1
	service=$2

	say "Add service stanza '$service' to $FINIT_CONF ..."
	texec sh -c "echo '$service' > $FINIT_CONF"

	say 'Reload Finit'
	#texec sh -c "initctl debug"
	texec sh -c "initctl reload"
	#texec sh -c "initctl status"
	#texec sh -c "ps"

	#texec sh -c "initctl status"
	#texec sh -c "ps"
	#texec sh -c "initctl status serv"

	retry 'assert_num_children 1 serv'
	retry "assert_pidfile $pidfn" 1

	say 'Stop the service'
	texec sh -c "initctl stop serv"

	retry 'assert_num_children 0 serv'

	say 'Start the service again'
	texec sh -c "initctl start serv"

	retry 'assert_num_children 1 serv'

	say "Done, drop service from $FINIT_CONF ..."
	texec sh -c "rm $FINIT_CONF"
	texec sh -c "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_one "/run/serv.pid"  "service pid:!/run/serv.pid /test_assets/serv -- Forking service"
# This one could never be started by and monitored by Finit: it forks to
# background and does not create a PID file.  Essentially it's lost to
# Finit, and any other sane process monitor.
#test_one "/run/serv.pid"  "service pid:/run/serv.pid  /test_assets/serv -p -- Forking service w/o PID file"
test_one "/run/serv.pid"  "service pid:/run/serv.pid  /test_assets/serv -n -- Foreground service w/o PID file"
test_one "/run/serv.pid"  "service                    /test_assets/serv -n -p -- Foreground service w/ PID file"
test_one "/run/servy.pid" "service pid:/run/servy.pid /test_assets/serv -n -p -P /run/servy.pid -- Foreground service w/ custom PID file"
