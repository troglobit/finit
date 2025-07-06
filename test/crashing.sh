#!/bin/sh
# Verifies that Finit calls post:script for crashing services

set -eu

TEST_DIR=$(dirname "$0")

test_teardown()
{
    say "Test done $(date)"

    say "Running test teardown."
    run "rm -f $FINIT_CONF" "/tmp/post"
}

crashit()
{
    nm=$1

    say 'Simulate service crash (kill -9 ..)'
    i=0
    laps=100
    while [ $i -lt $laps ]; do
	i=$((i + 1))
	say "Lap $i/$laps, killing service $nm ..." # we have this, no sleep needed
	if ! run "slay $nm"; then
	    break;
	fi
    done
}

test_one()
{
    type=$1
    shift
    nm=$1
    shift
    args=$*

    say "Add service stanza in $FINIT_CONF"
    run "echo service log:stderr oncrash:script post:/bin/post.sh $nm $args > $FINIT_CONF"

    say 'Reload Finit'
    run "initctl reload"

    if [ "$type" = "sig" ]; then
	retry "assert_num_children 1 $nm"
	run "initctl status $nm"
	crashit "$nm"
    fi

    retry "assert_status $nm crashed" 500
    run "cat /tmp/post"
    assert_file_contains "/tmp/post" "POST"
    run "rm -f /tmp/post"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"

test_one sig service.sh "       -- Test crashing service.sh"
test_one app serv       "-np -c -- Test crashing serv"
