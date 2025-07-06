#!/bin/sh
# Regression test for basic /etc/rc.local functionality
# shellcheck disable=2317,2034
set -eu

RCLOCAL="echo 'KILROY WAS HERE' > /tmp/rclocal"
TEST_DIR=$(dirname "$0")
#DEBUG=1

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

sleep 1
assert_file_contains /tmp/rclocal "KILROY WAS HERE"

exit 0
