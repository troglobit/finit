#!/bin/sh
#
# Verifies SysV runparts behavior: to run all scripts in a directory at
# the end of system bootstrap, right before calling /etc/rc.local:
#
#  - only SNNfoo or KNNfoo executable scripts should be called
#  - always in alphabetical order
#  - SNNfoo should be called with a 'start' argument
#  - KNNfoo should be called with a 'stop' argument
#
# shellcheck disable=2034

BOOTSTRAP="runparts sysv /etc/rcS.d"
TEST_DIR=$(dirname "$0")
DEBUG=1

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
texec cat /tmp/sysv.log
texec diff -u /usr/share/runparts/sysv.log /tmp/sysv.log
texec cmp /usr/share/runparts/sysv.log /tmp/sysv.log || fail "runparts sysv in wrong order"
