#!/bin/sh

set -eu

# Test
say 'Set up a service'
cp "$TEST_DIR"/service.sh test_root/bin
texec sh -c "echo 'service [2345] kill:20 log /bin/service.sh' > $FINIT_CONF"

say 'Reload Finit'
texec kill -SIGHUP 1
sleep 1

service_pids=$(texec pgrep -P 1 | wc -l)
assert 'One service is running' "$service_pids" -eq 1

say 'Remove the service'
texec sh -c "echo > $FINIT_CONF"

say 'Reload Finit'
texec kill -SIGHUP 1
sleep 1

service_pids=$(texec pgrep -P 1 | wc -l)
assert 'Zero services are running' "$service_pids" -eq 0

# Teardown
rm -f test_root/bin/service.sh
