#!/bin/sh
# Verify selecting alternate runlevel from cmdline
# shellcheck disable=SC2034
set -eu

TEST_DIR=$(dirname "$0")
FINIT_RUNLEVEL=9

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

say 'Check runlevel'
lvl=$(run "initctl runlevel | awk '{print \$2;}'")
if [ "$lvl" -eq "9" ]; then
    return 0
fi

return 1
