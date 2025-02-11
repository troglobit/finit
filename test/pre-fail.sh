#!/bin/sh
# Verify Finit:
#  - does not start main process when pre:script fails
#  - times out and kills long running pre:script
set -eu

#export DEBUG=true
TEST_DIR=$(dirname "$0")
# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

vrfy()
{
    nm=serv
    tmo=$1
    msg=$2
    err=$3

    run "echo 'service [2345] pre:$tmo,fail.sh $nm -n -- $msg' > $FINIT_CONF"
    run "initctl reload"

    retry "assert_status_full $nm 'crashed $err'" 50
}

#run "initctl debug"

vrfy 5 "Failing pre: script" "(code=exited, status=42)"
vrfy 1 "Timeout pre: script" "(code=signal, status=9/KILL)"
