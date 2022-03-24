#!/bin/sh

cleanup()
{
  echo "Got signal, stopping ..."
  rm -f /run/service.pid
  exit 0
}

# Hook SIGUSR1 and dump trace to file system
# shellcheck disable=SC2172
trap 'echo USR1 > /tmp/usr1.log' USR1
trap cleanup INT
trap cleanup TERM
trap cleanup QUIT
trap cleanup EXIT

echo $$ > /run/service.pid

# sleep may exit on known signal, so
# we cannot use 'set -e'
while true; do
  sleep 1
done
