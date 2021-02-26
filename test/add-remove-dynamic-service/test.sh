#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")/..

# shellcheck source=/dev/null
. "$TEST_DIR/lib.sh"

say 'environment:'
texec env
say 'initctl:'
texec busybox-x86_64 which initctl
say 'ldd initctl:'
texec sh -c 'LD_TRACE_LOADED_OBJECTS=1 initctl'
say 'finit:'
texec busybox-x86_64 which finit
say 'ldd finit:'
texec sh -c 'LD_TRACE_LOADED_OBJECTS=1 finit'
say 'tree:'
texec find / | grep -v '^/sys' | grep -v '^/proc'

test_teardown() {
    say "Test done $(date)"
    say "Running test teardown."

    texec rm -f "$FINIT_CONF"
    texec rm -f /test_assets/service.sh
}

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TESTENV_ROOT"/test_assets/

say "Add a dynamic service in $FINIT_CONF"
texec sh -c "echo 'service [2345] kill:20 log /test_assets/service.sh' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.conf'
texec sh -c "echo > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 0 service.sh'
