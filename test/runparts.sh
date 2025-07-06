#!/bin/sh
#
# Verifies basic runparts behavior: to run all scripts in a directory at
# the end of system bootstrap, right before calling /etc/rc.local:
#
#  - only executable scripts should be called
#  - always in alphabetical order
#  - SNNfoo should be called with a 'start' argument
#  - KNNfoo should be called with a 'stop' argument
#
# shellcheck disable=SC2034

BOOTSTRAP="runparts /etc/rc.d"
TEST_DIR=$(dirname "$0")
#DEBUG=1

test_teardown()
{
    say "Test done $(date)"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

while true; do
    lvl=$(texec initctl runlevel)
    say "Current runlevel $lvl"
    if [ "$lvl" = "N 2" ]; then
	break;
    fi
    sleep 1
done

sleep 1
texec diff -u /usr/share/runparts/ref.log /tmp/runparts.log
texec cmp /usr/share/runparts/ref.log /tmp/runparts.log || fail "runparts in wrong order"
