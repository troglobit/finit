#!/bin/sh
# Verifies initctl can send arbitrary signals (USR1 in this case) to finit managed services

set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
	texec rm -f /test_assets/service.sh
}

say "Test start $(date)"

cp "$TEST_DIR"/common/service.sh "$TENV_ROOT"/test_assets/

say 'Ensure file system is cleared'
texec rm -f /tmp/usr1.log

say "Add service stanza in $FINIT_CONF"
texec sh -c "echo 'service [2345] kill:20 log /test_assets/service.sh -- Test service' > $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"

say 'Send SIGUSR1 to service...'
texec sh -c "initctl signal service.sh SIGUSR1"

# shellcheck disable=SC2016
retry 'assert "service.sh received SIGUSR" "$(texec cat /tmp/usr1.log)" = "USR1"' 10 1
