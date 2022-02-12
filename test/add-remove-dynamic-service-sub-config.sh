#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown() {
    say "Test done $(date)"
    say "Running test teardown."

    texec rm -f "$FINIT_RCSD/service.conf"
    texec rm -f /test_assets/service.sh
}

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TENV_ROOT"/test_assets/

say "Add a dynamic service in $FINIT_RCSD/service.conf"
texec sh -c "echo 'service [2345] kill:20 log /test_assets/service.sh -- Dyn serv' > $FINIT_RCSD/service.conf"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Remove the dynamic service from /etc/finit.d/service.conf'
texec sh -c "echo > $FINIT_RCSD/service.conf"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 0 service.sh'
