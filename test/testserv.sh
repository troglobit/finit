#!/bin/sh
# Verify service readiness notification

set -eu
#set -x
TEST_DIR=$(dirname "$0")
#DEBUG=true

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

sleep 1
retry 'assert_norestart serv' 1 1

return 0
