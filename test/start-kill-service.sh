#!/bin/sh
# Verifies Finit restarts crashing services and registers their new PID
# from their (default) PID file.

set -eu

TEST_DIR=$(dirname "$0")

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

say "Test start $(date)"
rm -f "$SYSROOT"/oldpid

say "Add service stanza in $FINIT_CONF"
run "echo 'service [2345] respawn log:stderr service.sh -- Test service' > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"
#run "initctl status"
#run "initctl debug"

retry 'assert_num_children 1 service.sh'
run "initctl status service.sh"

say 'Simulate service crash (kill -9 ..)'
i=0
laps=1000
while [ $i -lt $laps ]; do
    i=$((i + 1))
    say "Lap $i/$laps, killing service ..." # we have this, no sleep needed
    run "slay service.sh"
done

retry 'assert_new_pid service.sh /run/service.pid'
retry 'assert_num_children 1 service.sh'
