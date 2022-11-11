#!/bin/sh
# Regression test for Finit bug #313:
#   - services get the 'forking' flag on initctl reload
#   - stopping a crashing task while its restart callback is pending
#     causes state 'running' with pid:0 (i.e., not running at all.)
# 

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
#	texec sh -c "initctl status -j serv"
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
}

say "Check deps ..."
check_dep jq

say "Test start $(date)"
rm -f "$TENV_ROOT"/oldpid

say "Add service stanza in $FINIT_CONF"
texec sh -c "echo 'service [2345] log:stderr serv -np -- Test service' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

say 'Verify serv is running ...'
retry 'assert_num_children 1 serv'

say 'Verify reload does not change forking type of service'
#texec sh -c "initctl status -j serv"
texec sh -c "initctl reload"
#texec sh -c "initctl status -j serv"
assert_forking serv false

say 'Simulate service crash (kill -9 ..)'
texec sh -c "initctl debug"
i=0
laps=7
while [ $i -lt $laps ]; do
	i=$((i + 1))
	say "Lap $i/$laps, killing service ..." # we have this, no sleep needed
	texec sh -c "slay serv"
done

say 'Verify stopping service actually stops it'
sleep 1
texec sh -c "initctl stop serv"
sleep 5
#texec sh -c "initctl status serv"
assert_status serv stopped

say 'Verify restarting service actually starts it'
texec sh -c "initctl start serv"
retry 'assert_num_children 1 serv'
