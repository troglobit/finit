#!/bin/sh
# Verify service readiness notification

set -eu
#set -x

TEST_DIR=$(dirname "$0")

test_setup()
{
	say "Test start $(date)"
	texec sh -c "mkdir -p /etc/default"
}

test_teardown()
{
	say "Test done $(date)"
	say "Running test teardown."

	texec rm -f "$FINIT_CONF"
}

test_one()
{
    type=$1
    service=$2

    num=$(texec sh -c "find /proc/1/fd |wc -l")
    say "finit: number of open file descriptors before test: $num"

    say "Add $type stanza to $FINIT_CONF"
    texec sh -c "echo $service > $FINIT_CONF"

    say 'Reload Finit'
    texec sh -c "initctl reload"

    sleep 1
    texec sh -c "initctl status serv"
    assert_status "serv" "running"

    sleep 2
    texec sh -c "initctl cond dump"
    assert_cond "service/serv/ready"

    say "Verify 'ready' is set after SIGHUP ..."
    texec sh -c "initctl reload serv"
    sleep 2
    assert_status "serv" "running"
    assert_cond "service/serv/ready"

    say "Verify 'ready' is reasserted if service crashes and is restarted ..."
    texec sh -c "initctl signal serv 9"
    sleep 2
    assert_status "serv" "running"
    assert_cond "service/serv/ready"

    say "Verify 'ready' is deasserted on stop ..."
    texec sh -c "initctl stop serv"
    sleep 1
    assert_status "serv" "stopped"
    assert_nocond "service/serv/ready"

    # sleep 2
    # num=$(texec sh -c "find /proc/1/fd |wc -l")
    # say "finit: number of open file descriptors after test: $num"
    
}

# shellcheck source=/dev/null
. "$TEST_DIR/tenv/lib.sh"

texec sh -c "initctl debug"

test_one "s6"      "service log:stdout notify:s6      serv -np -N %n -- s6 readiness"
test_one "systemd" "service log:stdout notify:systemd serv -np       -- systemd readiness"

test_one "s6"      "service log:stdout notify:s6      serv -np -N %n -- s6 readiness"
test_one "systemd" "service log:stdout notify:systemd serv -np       -- systemd readiness"

test_one "s6"      "service log:stdout notify:s6      serv -np -N %n -- s6 readiness"
test_one "systemd" "service log:stdout notify:systemd serv -np       -- systemd readiness"

test_one "s6"      "service log:stdout notify:s6      serv -np -N %n -- s6 readiness"
test_one "systemd" "service log:stdout notify:systemd serv -np       -- systemd readiness"

return 0
