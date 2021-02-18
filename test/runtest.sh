#!/bin/sh

set -eu

texec() {
    ./testenv_exec.sh "$(pgrep -P "$finit_ppid")" "$@"
}

fg_red='31'
fg_green='32'
fg_yellow='33'
log() {
    printf "< $TEST_DIR > \e[1;%dm%s\e[0m %s\n" "$1" "$2" "$3"
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
    texec kill 1

    wait
}

trap teardown EXIT

# Setup
./testenv_start.sh finit &
finit_ppid=$!
for i in $(seq 1 50); do
    sleep 0.1
    pgrep -P "$finit_ppid" > /dev/null || continue
    break
done

texec cat /dev/tty0 &
sleep 1

TEST_DIR=$(dirname "$1")

. "$1"
