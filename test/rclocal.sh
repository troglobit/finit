#!/bin/sh
# Regression test for basic /etc/rc.local functionality
# shellcheck disable=SC2317
set -eu

TEST_DIR=$(dirname "$0")
#DEBUG=1
# shellcheck disable=SC2034
RCLOCAL="echo 'KILROY WAS HERE' > /tmp/rclocal"

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

sleep 1
assert_file_contains /tmp/rclocal "KILROY WAS HERE"

exit 0
