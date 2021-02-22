#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")/..
# shellcheck source=/dev/null
. "$TEST_DIR/runtest.sh"

assert_num_children() {
    assert "$1 services are running" "$(texec pgrep -P 1 "$2" | wc -l)" -eq "$1"
}

# Test
say 'Set up a service'
cp "$TEST_DIR"/test_start_stop_service/service.sh "$TEST_DIR"/test_root/bin
texec sh -c "echo 'service [2345] kill:20 log /bin/service.sh' > $FINIT_CONF"

say 'Reload Finit'
texec kill -SIGHUP 1

retry 'assert_num_children 1 service.sh'

say 'Remove the service'
texec sh -c "echo > $FINIT_CONF"

say 'Reload Finit'
texec kill -SIGHUP 1

retry 'assert_num_children 0 service.sh'

# Teardown
rm -f test_root/bin/service.sh
