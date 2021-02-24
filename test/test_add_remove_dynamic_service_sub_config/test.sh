#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")/..
TESTENV_ROOT="$TEST_DIR"/test_root

# shellcheck source=/dev/null
. "$TEST_DIR/lib.sh"

test_teardown() {
    say "Test done $(date)"
    say "Running test teardown."

    remove_thing "$TESTENV_ROOT/$FINIT_CONF_DIR/service.conf"
    remove_thing "$TESTENV_ROOT"/test_assets/service.sh
}

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TEST_DIR"/test_root/test_assets/

say "Add a dynamic service in $FINIT_CONF_DIR/service.conf"
texec sh -c "echo 'service [2345] kill:20 log /test_assets/service.sh' > $FINIT_CONF_DIR/service.conf"

say 'Reload Finit'
texec kill -SIGHUP 1

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.d/service.conf'
texec sh -c "echo > $FINIT_CONF_DIR/service.conf"

say 'Reload Finit'
texec kill -SIGHUP 1

retry 'assert_num_children 0 service.sh'
