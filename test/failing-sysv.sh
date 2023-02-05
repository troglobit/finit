#!/bin/sh
# Verify what happens when sysv scripts start services that keep
# crashing -- hint: crashing sysv services should be detected by
# Finit just like regular services.
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

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

# This instructs serv to check the environment, and exit
# if it cannot find "xyzzy", thus triggering a premature
# exit which Finit should act on to retstart it.
say "Setting up bogus /etc/default/serv"
run "echo 'SERV_ARGS=\"-e xyzzy:lives\"' > /etc/default/serv"

say "Add sysv stanza to $FINIT_CONF"
run "echo 'sysv restart:5 [2345] pid:!/run/serv.pid name:serv /etc/init.d/S02-serv.sh -- Crashing SysV service' > $FINIT_CONF"

#run "initctl debug"

say 'Reload Finit'
run "initctl reload"
#run "initctl status serv"
#run "ps"

say 'Pending sysv restarts by Finit ...'
#run "initctl status serv"
#run "ps"
retry 'assert_restarts 5 serv' 20 1

return 0
