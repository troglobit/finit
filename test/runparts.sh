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

TEST_DIR=$(dirname "$0")
#DEBUG=1

# shellcheck disable=SC2034
BOOTSTRAP="runparts /etc/rc.d"

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

while true; do
    lvl=$(texec initctl runlevel)
    say "Current runlevel $lvl"
    if [ "$lvl" = "N 2" ]; then
	break;
    fi
    sleep 1
done

texec diff -u /usr/share/runlevel/ref.log /tmp/runparts.log
texec cmp /usr/share/runlevel/ref.log /tmp/runparts.log || fail "runparts in wrong order"
