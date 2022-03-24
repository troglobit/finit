#!/bin/sh

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

cp "$TEST_DIR"/common/service.sh     "$TENV_ROOT"/test_assets/
cp "$TEST_DIR"/common/S01-service.sh "$TENV_ROOT"/test_assets/
chmod +x "$TENV_ROOT"/test_assets/*.sh

say "Add sysv stanza in $FINIT_CONF"
texec sh -c "echo 'sysv [2345] pid:!/run/service.pid name:service.sh /test_assets/S01-service.sh -- SysV test service' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

retry 'assert_num_children 1 service.sh'

say 'Stop the sysv service'
texec sh -c "initctl stop service.sh"

retry 'assert_num_children 0 service.sh'

say 'Start the sysv service again'
texec sh -c "initctl start service.sh"

retry 'assert_num_children 1 service.sh'

return 0
