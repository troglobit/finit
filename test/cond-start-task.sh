#!/bin/sh
# A run/task runs once per runlevel, unless its .conf is touched
# then it will run again on initctl reload.  This test verifies
# this behavior, with the added twist of starting the task by
# asserting a condition.
set -eu

TEST_DIR=$(dirname "$0")

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

TEST_CONF=$FINIT_RCSD/cond.conf


test_setup()
{
	say "Test start $(date)"
}

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."
	run "rm -f $TEST_CONF"
}

test_one()
{
	cond=$1
	stanza=$2

	sleep 5
	say 'Enable finit debug, try to figure out what is going on ...'
	run "initctl debug"

	say "Add stanza '$stanza' to $TEST_CONF ..."
	run "echo '$stanza' > $TEST_CONF"

	run "ls -l /run/ /run/finit/"
	say 'Reload Finit'
	run "initctl ls"
	run "initctl reload"
	run "initctl status"
	run "ps"

	date
	sleep 2
	date
	run "ls -l /run/ /run/finit/"
	say 'Asserting condition'
	run "initctl cond set $cond"

	date
	sleep 2
	date
	run "ls -l /run/ /run/finit/"
	run "initctl status"
	run "ps"
	run "initctl status task.sh"

	sleep 1
	run "echo Hej; cat /run/task.state"

	assert_file_contains /run/task.state 1

	say 'Reload Finit'
	run "initctl reload"
	sleep 1
	say 'Ensure task has not run again ...'
	assert_file_contains /run/task.state 1

	say 'Switch to another runlevel ...'
	run "initctl runlevel 3"
	sleep 1
	say 'Ensure task has run again ...'
	assert_file_contains /run/task.state 2
	
	say 'Touch task.sh .conf file and reload Finit ...'
	run "touch $TEST_CONF"
	run "initctl reload"
	sleep 1
#	run "initctl status task.sh"
	assert_file_contains /run/task.state 3

	say "Done, drop stanza from $TEST_CONF ..."
	run "rm $TEST_CONF"
	run "initctl reload"
}

test_one "hello" "task <usr/hello> task.sh -- Hello task"
