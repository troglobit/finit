#!/bin/sh
# A run/task runs once per runlevel, unless its .conf is touched
# then it will run again on initctl reload.  This test verifies
# this behavior, with the added twist of starting the task by
# asserting a condition.
set -eu

TEST_CONF=/etc/finit.d/cond.conf
TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
}

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."
	texec rm -f "$TEST_CONF"
}

test_one()
{
	cond=$1
	stanza=$2

	say "Add stanza '$stanza' to $TEST_CONF ..."
	texec sh -c "echo '$stanza' > $TEST_CONF"

	texec sh -c "ls -l /run/ /run/finit/"
	say 'Reload Finit'
	texec sh -c "initctl debug"
	texec sh -c "initctl ls"
	texec sh -c "initctl reload"
	texec sh -c "initctl status"
#	texec sh -c "ps"

	date
	sleep 2
	date
	texec sh -c "ls -l /run/ /run/finit/"
	say 'Asserting condition'
	texec sh -c "initctl cond set $cond"

	date
	sleep 2
	date
	texec sh -c "ls -l /run/ /run/finit/"
	texec sh -c "initctl status"
	texec sh -c "ps"
	texec sh -c "initctl status task.sh"

	sleep 1
	texec sh -c "echo Hej; cat /run/task.state"

	assert_file_contains /run/task.state 1

	say 'Reload Finit'
	texec sh -c "initctl reload"
	sleep 1
	say 'Ensure task has not run again ...'
	assert_file_contains /run/task.state 1

	say 'Switch to another runlevel ...'
	texec sh -c "initctl runlevel 3"
	sleep 1
	say 'Ensure task has run again ...'
	assert_file_contains /run/task.state 2
	
	say 'Touch task.sh .conf file and reload Finit ...'
	texec sh -c "touch $TEST_CONF"
	texec sh -c "initctl reload"
	sleep 1
#	texec sh -c "initctl status task.sh"
	assert_file_contains /run/task.state 3

	say "Done, drop stanza from $TEST_CONF ..."
	texec sh -c "rm $TEST_CONF"
	texec sh -c "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_one "hello" "task <usr/hello> task.sh -- Hello task"
