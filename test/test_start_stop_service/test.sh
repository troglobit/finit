#!/bin/sh

set -eu

assert_num_children() {
    assert "$1 services are running" "$(texec pgrep -P 1 "$2" | wc -l)" -eq "$1"
}

# Test
say 'Set up a service'
cp "$TEST_DIR"/service.sh test_root/bin
texec mkdir -p "$(dirname "$FINIT_CONF")"
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
