#!/bin/sh
# Verify that the status command 'initctl status foo' matches three
# instances of tasks called foo:1, foo:2, and foo:3.  It should not
# match the fourth task called foobar.  This is a regression test to
# ensure issue #275 doesn't happen again.
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

test_init()
{
	texec sh -c "echo '# Three foo and one bar enter Finit' > $FINIT_CONF"
}

test_add_one()
{
	service=$1

	say "Add service stanza '$service' to $FINIT_CONF ..."
	texec sh -c "echo '$service' >> $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

test_init

test_add_one "task manual:yes name:foo :1 serv -- Foo \#1"
test_add_one "task manual:yes name:foo :2 serv -- Foo \#2"
test_add_one "task manual:yes name:foo :3 serv -- Foo \#3"
test_add_one "task manual:yes name:foobar serv -- Foobar"

say 'Reload Finit'
#texec sh -c "initctl debug"
texec sh -c "initctl reload"
texec sh -c "initctl status"

#texec sh -c "initctl status"
#texec sh -c "ps"
#texec sh -c "initctl status serv"

assert_num_services 3 foo
assert_desc "Foobar" foobar
assert_desc "Foo #1" foo:1

