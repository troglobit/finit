#!/bin/sh
# Ensure service environment variables are sourced and expanded properly.
set -e

TEST_DIR=$(dirname "$0")

test_setup()
{
    say "Test start $(date)"
}

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

sep "/etc/env:"
run "cat /etc/env"
run "echo 'service env:/etc/env serv -np -e xyzzy:bar -- serv checks xyzzy=bar' > $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

retry 'assert_num_children 1 serv'

say "Done, drop service from $FINIT_CONF ..."
run "rm $FINIT_CONF"
run "initctl reload"
