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
}

say "Test start $(date)"

say 'Ensure file system is cleared'
run "rm -f /tmp/usr1.log"

# Verifies line continuation as well
say "Add service stanza in $FINIT_CONF"
run "echo 'service [2345] kill:20 \\
  log service.sh \\
   -- Test service' > $FINIT_CONF"
run "cat $FINIT_CONF"
#run "initctl debug"

say 'Reload Finit'
run "initctl reload"

#run "initctl status service.sh"

say 'Send SIGUSR1 to service...'
run "initctl signal service.sh SIGUSR1"

# shellcheck disable=SC2016
retry 'assert "service.sh received SIGUSR" "$(texec cat /tmp/usr1.log)" = "USR1"' 10 1
