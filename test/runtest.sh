#!/bin/sh

set -eu

texec() {
    ./testenv_exec.sh "$(pgrep -P "$finit_ppid")" "$@"
}

color_reset='\e[0m'
fg_red='\e[1;31m'
fg_green='\e[1;32m'
fg_yellow='\e[1;33m'
log() {
    printf "< $TEST_DIR > %b%b%b %s\n" "$1" "$2" "$color_reset" "$3"
}

assert() {
    msg=$1
    shift
    if [ ! "$@" ]; then
        log "$fg_red" ✘ "$msg"
        exit 1
    fi
    log "$fg_green" ✔ "$msg"
}

say() {
    log "$fg_yellow" ● "$@"
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

TEST_DIR=$(dirname "$1")

trap teardown EXIT

log "$color_reset" 'Test start' ''
log "$color_reset" '--' ''

# Setup
./testenv_start.sh finit &
finit_ppid=$!
for _ in $(seq 1 50); do
    sleep 0.1
    pgrep -P "$finit_ppid" > /dev/null || continue
    break
done

texec cat /dev/tty0 &
sleep 1

# shellcheck source=/dev/null
. "$TEST"
