#!/bin/sh

cleanup()
{
    echo "Got signal, stopping ..."
    rm -f /run/service.pid
    exit 0
}

reload()
{
    echo
    echo "Ready."
    echo
    sleep 1
    echo $$ > /run/service.pid
}

# Hook SIGUSR1 and dump trace to file system
# shellcheck disable=SC2172
trap 'echo USR1 > /tmp/usr1.log' USR1
trap reload  HUP
trap cleanup INT
trap cleanup TERM
trap cleanup QUIT
trap cleanup EXIT

reload

# sleep may exit on known signal, so
# we cannot use 'set -e'
while true; do
    sleep 1
done
