#!/bin/sh
# Regression test for issue #392

set -eu
#set -x

TEST_DIR=$(dirname "$0")

test_setup()
{
    say "Test start $(date)"
    run "mkdir -p /etc/default"
}

test_teardown()
{
    say "Test done $(date)"
    say "Running test teardown."

    run "rm -f $FINIT_CONF"
}

srv()
{
    say "Adding service $(echo "$1" | sed -n -e 's/^.*-- //p')"
    run "echo '$1' >> $FINIT_CONF"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"

# Test systemd notify
srv 'service name:test1_systemd [2345] notify:systemd restart:0                 serv -i test1 -n  -- Systemd proc'
srv 'service name:test2         [2345]            <service/test1_systemd/ready> serv -i test2 -np -- Dependency test 1'

# Test pid notify
srv "service name:test3_pid     [2345] notify:pid <service/test1_systemd/ready> serv -i test3 -np -- Dependency test 2"
srv "service name:test4         [2345]            <service/test3_pid/ready>     serv -i test4 -np -- Dependency test 3"

sep "$FINIT_CONF"
run "cat $FINIT_CONF"
sep

say 'Reload Finit'
run "initctl reload"

say "Waiting for all services to start"
retry 'assert_status "test4" "running"' 5 1

# Load bearing debug.  Without this the bug cannot be reproduced!
run "initctl debug"

sep
say "Stopping root service, all others should also stop"
run "initctl stop test1_systemd"
retry 'assert_status "test3_pid" "waiting"' 5 1

sep
say "Verify test4 does not restart"
run "initctl status"
run "initctl cond dump"
assert_status "test4" "waiting"

return 0
