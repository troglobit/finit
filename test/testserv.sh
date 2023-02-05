#!/bin/sh
# Verify service readiness notification
# shellcheck disable=SC2034

set -eu
#set -x

#DEBUG=true
FINIT_ARGS="finit.cond=testserv"
TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

run "initctl"
run "initctl status testserv"
run "initctl cond dump"
sleep 1
retry 'assert_norestart testserv' 1 1

return 0
