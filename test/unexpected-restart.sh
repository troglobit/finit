#!/bin/sh
# A service (B) going into flux, 'initctl reload' with no changes to
# activate, should do not cause ripple effects removing any service
# conditions, e.g., <service/B/running>.  During reload all services
# are paused while Finit checks for any service3 changes to activate,
# which may mean stopping or restarting existing services. 
#
# Issue #382
set -e

#DEBUG=true
TEST_DIR=$(dirname "$0")

test_setup()
{
    run "cat >> $FINIT_CONF" <<EOF
service log:stdout name:A serv -np -i A
service log:stdout notify:systemd <pid/A>           name:B serv -np -i B -N 0 -- B needs A
service log:stdout notify:systemd <service/B/ready> name:C serv -np -i C -N 0 -- C needs B(service)
service log:stdout notify:systemd <pid/B>           name:D serv -np -i D -N 0 -- D needs B(pid)
task <service/C/ready,service/D/ready>              name:allup /sbin/initctl cond set allup -- Everything is up
EOF
    say "Test start $(date)"
}

test_teardown()
{
    say "Test done $(date)"
}

pidof()
{
    texec initctl -j status "$1" | jq .pid
}

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

sep "$FINIT_CONF"
run "cat $FINIT_CONF"
sep
run "initctl reload"
run "initctl status"
sep

say "waiting for primary startup to complete"
retry 'assert_status allup "done"' 100 1
assert_status C "running"
oldpid=$(pidof C)
assert_status D "running"
doldpid=$(pidof D)

sep "pre-reload status"
run "initctl status"
sep

say "Reload Finit, who gets restarted?"
#run "initctl debug"
run "initctl reload"
sleep 2

assert_status A "running"
assert_status B "running"
assert_status C "running"
assert_status D "running"

newpid=$(pidof C)
# shellcheck disable=SC2086
assert "C was not restarted" $oldpid -eq $newpid

dnewpid=$(pidof D)
# shellcheck disable=SC2086
assert "D was not restarted" $doldpid -eq $dnewpid
