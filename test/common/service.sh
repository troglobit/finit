#!/bin/sh

set -eu

# Hook SIGUSR1 and dump trace to file system
# shellcheck disable=SC2172
trap 'echo USR1 > /tmp/usr1.log' 10

echo $$ > /run/service.pid

while true; do
  sleep 5
done

rm /run/service.pid
