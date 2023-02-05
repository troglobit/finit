#!/bin/sh
set -eu

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

test_one()
{
	pre=$1
	precont=$2
	service=$3
	post=$4
	postcont=$5

	say "Add service stanza '$service' to $FINIT_CONF ..."
	run "echo '$service' > $FINIT_CONF"

	say 'Reload Finit'
	run "initctl reload"

	retry 'assert_num_children 1 serv'
	if [ -n "$pre" ]; then
		assert_file_contains "$pre" "$precont"
		run "rm -f $pre"
	fi

	say 'Stop the service'
	run "initctl stop serv"

	retry 'assert_num_children 0 serv'
	if [ -n "$post" ]; then
		assert_file_contains "$post" "$postcont"
		run "rm -f $post"
	fi

	say "Done, drop service from $FINIT_CONF ..."
	run "rm $FINIT_CONF"
	run "initctl reload"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

#run "initctl debug"

test_one ""       ""  "service                   serv -np -- Regular fg service, no pre/post scripts" "" ""
test_one ""       ""  "service env:/etc/env      serv -np -e foo:bar -- serv + env, no pre/post scripts" "" ""
test_one /tmp/pre PRE "service pre:/bin/pre.sh   serv -np -- serv + pre script" "" ""
test_one /tmp/pre bar "service env:/etc/env pre:/bin/pre.sh serv -np -e baz:qux -- Env + pre script" "" ""
test_one ""       ""  "service post:/bin/post.sh serv -np -- Regular fg service, post script" /tmp/post POST
test_one ""       ""  "service env:/etc/env post:/bin/post.sh serv -np -- Regular fg service, post script" /tmp/post qux
