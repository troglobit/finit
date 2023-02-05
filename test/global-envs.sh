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

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"
say "Add custom envs to $FINIT_CONF ..."
run "echo 'foo=bar' >  $FINIT_CONF"
run "echo 'baz=qux' >> $FINIT_CONF"

say "Add service stanza to $FINIT_CONF ..."
run "echo 'service serv -np -e foo:bar -- Verify foo=bar' >> $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Modify $FINIT_CONF slightly ..."
run "sed -i 's/foo:bar/baz:qux/; s/foo=bar/baz=qux/' $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Swap envs completely in $FINIT_CONF ..."
run "echo 'bar=foo' >  $FINIT_CONF"
run "echo 'qux=baz' >> $FINIT_CONF"

say "Add swapped service stanza to $FINIT_CONF ..."
run "echo 'service serv -np -e bar:foo -E baz -- Verify bar=foo and no baz' >> $FINIT_CONF"

say 'Reload Finit'
run "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Done, drop service from $FINIT_CONF ..."
run "rm $FINIT_CONF"
run "initctl reload"
