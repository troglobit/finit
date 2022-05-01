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

	texec rm -f "$FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

#texec sh -c "initctl debug"
say "Add custom envs to $FINIT_CONF ..."
texec sh -c "echo 'foo=bar' >  $FINIT_CONF"
texec sh -c "echo 'baz=qux' >> $FINIT_CONF"

say "Add service stanza to $FINIT_CONF ..."
texec sh -c "echo 'service serv -np -e foo:bar -- Verify foo=bar' >> $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Modify $FINIT_CONF slightly ..."
texec sh -c "sed -i 's/foo:bar/baz:qux/; s/foo=bar/baz=qux/' $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Swap envs completely in $FINIT_CONF ..."
texec sh -c "echo 'bar=foo' >  $FINIT_CONF"
texec sh -c "echo 'qux=baz' >> $FINIT_CONF"

say "Add swapped service stanza to $FINIT_CONF ..."
texec sh -c "echo 'service serv -np -e bar:foo -E baz -- Verify bar=foo and no baz' >> $FINIT_CONF"

say 'Reload Finit'
texec sh -c "initctl reload"
sleep 1
retry 'assert_num_children 1 serv'

say "Done, drop service from $FINIT_CONF ..."
texec sh -c "rm $FINIT_CONF"
texec sh -c "initctl reload"
