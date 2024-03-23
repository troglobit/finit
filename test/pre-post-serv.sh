#!/bin/sh
#
# Verify pre:script and post:script runs as intended when starting,
# stopping and disabling services in various configurations and
# conditions.
#

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

startit()
{
    service=$1
    conf=$2
    pre=$3
    precont=$4

    say "Add stanza '$service' to $conf ..."
    run "echo '$service' > $conf"

    if echo "$conf" |grep -q available; then
	# configlets need must be enabled
	run "initctl enable serv"
    fi

    say 'Reload Finit'
    run "initctl reload"

    # Ensure service has started before checking pre condition
    retry 'assert_num_children 1 serv'

    # Verify pre:script has run
    if [ -n "$pre" ]; then
	assert_file_contains "$pre" "$precont"
	# Drop pre condition indicator for next test
	run "rm -f $pre"
    fi
}

stopit()
{
    conf=$1
    post=$2
    postcont=$3

    if echo "$conf" |grep -q available; then
	say 'Disable service & reload'
	run "initctl disable serv"
	run "initctl reload"
    else
	say 'Stop the service'
	run "initctl stop serv"
    fi

    # Ensure service has stopped before checking for post condition
    retry 'assert_num_children 0 serv'

    # Verify post:script has run
    if [ -n "$post" ]; then
	assert_file_contains "$post" "$postcont"
	# Drop post condition indicator for next test
	run "rm -f $post"
    fi

    say "Done, drop $conf ..."
    run "rm $conf"
    run "initctl reload"
}

test_one()
{
    pre=$1
    precont=$2
    service=$3
    post=$4
    postcont=$5
    avail="$FINIT_RCSD/available/serv.conf"

    sep "Stage 1/2"
    startit "$service" "$FINIT_CONF" "$pre" "$precont"
    stopit "$FINIT_CONF" "$post" "$postcont"

    sep "Stage 2/2"
    startit "$service" "$avail" "$pre" "$precont"
    stopit "$avail" "$post" "$postcont"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"

test_one ""       ""  "service                   serv -np -- Regular fg service, no pre/post scripts" "" ""
test_one ""       ""  "service env:/etc/env      serv -np -e foo:bar -- serv + env, no pre/post scripts" "" ""
test_one /tmp/pre PRE "service pre:/bin/pre.sh   serv -np -- serv + pre script" "" ""
test_one /tmp/pre bar "service env:/etc/env pre:/bin/pre.sh serv -np -e baz:qux -- Env + pre script" "" ""
test_one ""       ""  "service post:/bin/post.sh serv -np -- Regular fg service, post script" /tmp/post POST
test_one ""       ""  "service env:/etc/env post:/bin/post.sh serv -np -- Regular fg service, post script" /tmp/post qux
