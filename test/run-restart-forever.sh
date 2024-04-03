#!/bin/sh
# Regression test for issue #351 - verify that a dirty run/task in
# runlevel S is not restarted forever just because it exits with a
# non-zero error code.
set -eu

#export DEBUG=true
TEST_DIR=$(dirname "$0")

test_setup()
{
    say "Test start $(date)"
    run "mkdir -p /etc/default"
    export TEST_TIMEOUT=5
}

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

test_one()
{
    num=$1

    say "Reloading Finit ..."
    run "initctl reload"
#    run "initctl status fail.sh"

    say "Reloading Finit ..."
    run "initctl reload"
#    run "initctl status fail.sh"

    assert_restart_cnt "$num" "0/10" fail.sh
}

sep
say "Add stanza to $FINIT_CONF"
run "echo 'run fail.sh -- Failure' > $FINIT_RCSD/fail.conf"
run "echo 'run initctl touch fail' > $FINIT_RCSD/touch.conf"
test_one 2

sep
say "Changing runlevel ..."
run "initctl runlevel 3"
test_one 4

return 0
