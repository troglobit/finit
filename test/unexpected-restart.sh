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
service log:stdout notify:pid                       name:A serv -np -i A              -- A         (pid)
service log:stdout notify:systemd <pid/A>           name:B serv -np -i B              -- B needs A (systemd)
service log:stdout notify:s6      <service/B/ready> name:C serv -np -i C -N %n        -- C needs B (s6)
service log:stdout notify:none    <pid/A>           name:D type:forking serv -i D     -- D needs A (forking)
task <service/C/ready,service/D/ready>              name:allup initctl cond set allup -- Everything is up
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
run "initctl cond dump"
sep

run "initctl debug"
say "waiting for primary startup to complete"
retry 'assert_status allup "done"' 10 1
assert_status C "running"
oldpid=$(pidof C)

sep "pre-reload status"
run "initctl status"
run "initctl cond dump"
sep

say "Reload Finit, who gets restarted?"
#run "initctl debug"
run "initctl reload"
sleep 2

sep "post-reload status"
run "initctl status"
run "initctl cond dump"
sep

assert_status A "running"
assert_status B "running"
assert_status C "running"

newpid=$(pidof C)
# shellcheck disable=SC2086
assert "C was not restarted" $oldpid -eq $newpid
