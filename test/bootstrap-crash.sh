#!/bin/sh
# Regression test for issue #351.  A run/task that never completes
# successfully in runlevel S should not respawn forever.
#set -eu

TEST_DIR=$(dirname "$0")
#DEBUG=1
# shellcheck disable=SC2034
BOOTSTRAP="run [S] crasher.sh -- Party crasher"

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

test_setup()
{
    say "Test start $(date)"
}

test_teardown()
{
    say "Test done $(date)"
}

say "Hello SMTP Spoken here"
run initctl
run initctl status crasher.sh

sleep 10
say "We are done"
