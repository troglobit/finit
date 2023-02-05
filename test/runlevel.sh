#!/bin/sh
# Verify selecting alternate runlevel from cmdline
set -eu

TEST_DIR=$(dirname "$0")

# shellcheck disable=SC2034
FINIT_RUNLEVEL=9

test_setup()
{
	say "Test start $(date)"
}

test_teardown()
{
	say "Test done $(date)"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

say 'Check runlevel'
lvl=$(run "initctl runlevel | awk '{print \$2;}'")
if [ "$lvl" -eq "9" ]; then
	return 0
fi

return 1
