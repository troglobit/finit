#!/bin/sh
# Verify service readiness notification

set -eu
#set -x

TEST_DIR=$(dirname "$0")

test_setup()
{
    run "mkdir -p /etc/default"
}

test_teardown()
{
    say "Running test teardown."

    run "rm -f $FINIT_CONF"
}

test_one()
{
    type=$1
    service=$2

    # Skip systemd tests if serv doesn't have libsystemd support
    if [ "$type" = "systemd" ]; then
        if ! "$TEST_DIR/src/serv" -C | grep -q "libsystemd"; then
	    sep
            say "Skipping systemd test - serv built without libsystemd support"
            return 0
        fi
    fi

    sep

#    num=$(run "find /proc/1/fd |wc -l")
#    say "finit: number of open file descriptors before test: $num"

    say "Testing $(echo "$service" | sed -n -e 's/^.*-- //p')"
    run "echo $service > $FINIT_CONF"

    say 'Reload Finit'
    run "initctl reload"

    sleep 1
    #    run "initctl status serv"
    assert_status "serv" "running"

    sleep 2
    #    run "initctl cond dump"
    assert_cond "service/serv/ready"

    say "Verify 'ready' is set after SIGHUP ..."
    run "initctl reload serv"
    sleep 2
    assert_status "serv" "running"
    assert_cond "service/serv/ready"

    # Issue #343
    say "Verify 'ready' is reasserted if service crashes and is restarted ..."
    #    run "initctl status serv"
    run "initctl signal serv 9"
    retry 'assert_status "serv" "running"' 2 1
    retry 'assert_cond "service/serv/ready"' 3 1

    # Issue #343
    say "Verify 'ready' is reasserted on 'initctl restart serv' ..."
    run "initctl restart serv"
    sleep 1
    retry 'assert_status "serv" "running"'
    retry 'assert_cond "service/serv/ready"' 5 0.5

    say "Verify 'ready' is reasserted on 'initctl reload' ..."
    run "initctl reload"
    sleep 1
    #    run "initctl; initctl cond dump"
    assert_status "serv" "running"
    assert_cond "service/serv/ready"

    # Issue #343; pidfile.so inadvertently marked systemd services as
    # 'started' when they created their PID file.  The serv test daemon
    # waits 3 sec between pidfile and notify to trigger this bug.
    if [ "$type" != "pid" ]; then
	say "Verify 'ready' is *not* asserted immediately on restart + reload"
	run "initctl restart serv"
	run "initctl reload"
	sleep 1
	assert_nocond "service/serv/ready"
	retry 'assert_cond "service/serv/ready"' 5 0.5
    fi

    say "Verify 'ready' is deasserted on stop ..."
    run "initctl stop serv"
    sleep 1
    assert_status "serv" "stopped"
    assert_nocond "service/serv/ready"

    say "Cleaning up after test."
    run "rm -f $FINIT_CONF"
    run "initctl reload"

#    sleep 2
#    num=$(run "find /proc/1/fd |wc -l")
#    say "finit: number of open file descriptors after test: $num"
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

#run "initctl debug"

test_one "pid"     "service log:stdout notify:pid     serv -np       -- pid file readiness"
test_one "s6"      "service log:stdout notify:s6      serv -n -N %n  -- s6 readiness"
test_one "systemd" "service log:stdout notify:systemd serv -n        -- systemd readiness"

test_one "s6"      "service log:stdout notify:s6      serv -np -N %n -- s6 readiness with pidfile"
test_one "systemd" "service log:stdout notify:systemd serv -np       -- systemd readiness with pidfile"

return 0
