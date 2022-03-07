#!/bin/sh
# Verifies Finit restarts crashing services and registers their new PID
# from their (default) PID file.

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown() {
    say "Test done $(date)"
    say "Running test teardown."

    texec rm -f "$FINIT_CONF"
    texec rm -f /test_assets/service.sh
}

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TENV_ROOT"/test_assets/

say "Add service stanza in $FINIT_CONF"
texec sh -c "echo 'service [2345] respawn log /test_assets/service.sh -- Test service' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"
#texec sh -c "initctl status"

retry 'assert_num_children 1 service.sh'
texec sh -c "initctl status service.sh"

say 'Simulate service crash (kill -9 ..)'
i=0
laps=1000
while [ $i -lt $laps ]; do
    i=$((i + 1))
    say "Lap $i/$laps, killing service ..." # we have this, no sleep needed
    #    texec sh -c "echo \"PID is \$(initctl status service.sh |awk '/PID : /{ print \$3; }')\"; echo \"PID file says \$(cat /run/service.pid)\"; kill -9 \$(cat /run/service.pid)"
    #texec sh -c "initctl status service.sh"
    #texec sh -c "initctl signal service.sh KILL"
    texec sh -c "kill -9 \$(cat /run/service.pid)"
done

retry 'assert_new_pid service.sh /run/service.pid'
retry 'assert_num_children 1 service.sh'
