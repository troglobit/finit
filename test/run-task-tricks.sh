#!/bin/sh
#
# Verifies that a run task can call initctl at bootstrap and runlevel 2,
# without blocking, performing several tasks:
#
#  1. Query running services
#  2. Stop/Start a running service
#  3. Restart a service
#

TEST_DIR=$(dirname "$0")
#DEBUG=1

# shellcheck disable=SC2034
BOOTSTRAP="run [S] /bin/ready.sh                         -- Ready steady go\n\
run [S] name:one   initctl                    -- Query status\n\
service [S234] /sbin/service.sh               -- Background service\n\
run [2] name:two   initctl stop    service.sh -- Stopping service\n\
run [3] name:three initctl start   service.sh -- Starting service again\n\
run [4] name:four  initctl restart service.sh -- Restart service"
# shellcheck disable=SC2034
RCLOCAL="echo \"$0 ==============\" &1>2"

# shellcheck source=/dev/null
. "$TEST_DIR/lib/setup.sh"

say "$FINIT_CONF:"
run "cat $FINIT_CONF"
say "/etc/rc.local:"
run "cat /etc/rc.local"

say "Waiting for runlevel 2"
sleep 2
assert_status "service.sh" "stopped"

say "Changing to runlevel 3"
run "initctl runlevel 3"
sleep 2
assert_status "service.sh" "running"
run "initctl"
oldpid=$(texec initctl |awk '/service.sh/{print $1}')

say "Changing to runlevel 4"
run "initctl runlevel 4"
sleep 2
assert_status "service.sh" "running"
assert_pidiff "service.sh" "$oldpid"

run "initctl"
