#!/bin/sh
# Verify handling of dependant services, bug #314
# Two services: foo and bar, where bar depends on foo:
#
#   - bar must not start until foo is ready
#   - bar must be stopped when foo goes down
#   - bar must be restarted when foo is restarted
#
# Regression test bug #314, that bar is not in a restart loop when foo
# is stopped.  Also, verify that changing between runlevels, where foo
# is not allowed to run, but bar is, that bar is stopped also for that.

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

	texec rm -f "$FINIT_CONF" "/tmp/post"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

#run "initctl debug"
#run "ls -l /run/finit/cond/pid/ /lib/finit/plugins/"

run "echo service log:stderr name:foo             serv -np -i foo -f /tmp/arrakis  > $FINIT_CONF"
run "echo service log:stderr name:bar \<pid/foo\> serv -np -i bar -F /tmp/arrakis >> $FINIT_CONF"
run "cat $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"

assert_status "foo" "running"
assert_cond   "pid/foo"
assert_status "bar" "running"

#run "initctl status bar"
sleep 3
run "initctl stop foo"
assert_status "bar" "waiting"
