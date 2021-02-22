#!/bin/sh

set -eux

texec() {
    ./testenv_exec.sh "$finit_pid" "$@"
}

color_reset='\e[0m'
fg_red='\e[1;31m'
fg_green='\e[1;32m'
fg_yellow='\e[1;33m'
log() {
    printf "< $TEST_DIR > %b%b%b %s\n" "$1" "$2" "$color_reset" "$3"
}

assert() {
    __assert_msg=$1
    shift
    if [ ! "$@" ]; then
        log "$fg_red" ✘ "$__assert_msg ($*)"
        exit 1
    fi
    log "$fg_green" ✔ "$__assert_msg"
}

say() {
    log "$fg_yellow" • "$@"
}

teardown() {
    test_status="$?"
    log "$color_reset" '--' ''
    if [ "$test_status" -eq 0 ]; then
        log "$fg_green" 'TEST PASS' ''
    else
        log "$fg_red" 'TEST FAIL' ''
    fi
    texec kill 1

    wait
}

TEST="$1"
TEST_DIR=$(dirname "$TEST")
shift

trap teardown EXIT

FINIT_CONF=$(grep FINIT_CONF "$(dirname "$0")/../config.h" | cut -d' ' -f3 | cut -d'"' -f2)

log "$color_reset" 'Test start' ''
log "$color_reset" '--' ''


>&2 echo Environment:
>&2 ./testenv_start.sh env
>&2 echo --
>&2 echo Filesystem:
>&2 ./testenv_start.sh find / -path /sys -prune -o -print -path /proc -prune -o -print
>&2 echo --
>&2 echo "FINIT_CONF='$FINIT_CONF"

# Setup
./testenv_start.sh finit &
finit_ppid=$!
for _ in $(seq 1 50); do
    sleep 0.1
    finit_pid=$(pgrep -P "$finit_ppid")
    if [ "$finit_pid" ]; then
        break
    fi
done

tty=/dev/$(texec cat /sys/class/tty/console/active)
texec cat "$tty" &
sleep 1

# shellcheck source=/dev/null
. "$TEST"
