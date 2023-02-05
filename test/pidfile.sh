#!/bin/sh
# Verify parsing of pid: argument

set -eu

TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
}

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	run "rm -f $FINIT_CONF" "/tmp/post"
}

test_one()
{
	pidfn=$1
	service=$2

	sep
	say "Add service stanza '$service' to $FINIT_CONF ..."
	run "echo '$service' > $FINIT_CONF"
	run "initctl reload"

	assert_is_pidfile "serv" "$pidfn"

	say "Done, drop service from $FINIT_CONF ..."
	run "rm $FINIT_CONF"
	run "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"
#run "ls -l /run/finit/cond/pid/ /lib/finit/plugins/"

test_one "/run/serv.pid" "service serv -np"
test_one "/run/serv.pid" "service pid:!/run/serv.pid serv"
test_one "/run/serv.pid" "service type:forking serv"
test_one "/run/serv.pid" "service pid:!//run/serv.pid serv -np"
test_one "/run/serv.pid" "service pid://run/serv.pid serv -n"
test_one "/run/serv.pid" "service pid://../run/serv.pid serv -n"
test_one "/run/serv.pid" "service pid://run/../run//serv.pid serv -n"

sep
