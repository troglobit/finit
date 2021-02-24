#!/bin/sh

texec() {
    # shellcheck disable=SC2154
    TESTS_ROOT=. "$TEST_DIR/testenv_exec.sh" "$finit_pid" "$@"
}

pause() {
    echo "Press any key to continue... "
    read -r REPLY
}

toggle_finit_debug() {
    say 'Toggle finit debug'
    texec sh -c "/bin/initctl debug"
    sleep 0.5
}

color_reset='\e[0m'
fg_red='\e[1;31m'
fg_green='\e[1;32m'
fg_yellow='\e[1;33m'
log() {
    printf "< TEST > %b%b%b %s\n" "$1" "$2" "$color_reset" "$3"
}

assert() {
    __assert_msg=$1
    shift
    if [ ! "$@" ]; then
        log "$fg_red" ✘ "$__assert_msg ($*)"
        return 1
    fi
    log "$fg_green" ✔ "$__assert_msg"
}

retry() {
    __retry_cmd=$1
    shift
    case "$#" in
        2)
            __retry_n=$1
            __retry_sleep=$2
            ;;
        1)
            __retry_n=$1
            __retry_sleep=0.1
            ;;
        *)
            __retry_n=50
            __retry_sleep=0.1
            ;;
    esac

    for _ in $(seq 1 "$__retry_n"); do
        sleep "$__retry_sleep"
        __retry_cmd_out=$(eval "$__retry_cmd") && \
            echo "$__retry_cmd_out" && \
            return 0
    done
    __retry_cmd_exit="$?"
    echo "$__retry_cmd_out"
    return "$__retry_cmd_exit"
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
    if [ -n "${finit_pid+x}" ]; then
        texec kill -SIGUSR2 1
    fi

    wait
}

trap teardown EXIT

# shellcheck source=/dev/null
. "$TESTS_ROOT/../test.env"

log "$color_reset" 'Test start' ''
log "$color_reset" '--' ''

# Setup
"$TEST_DIR/testenv_start.sh" finit &
finit_ppid=$!
finit_pid=$(retry "pgrep -P $finit_ppid")

tty=/dev/$(texec cat /sys/class/tty/console/active)
texec cat "$tty" &
sleep 1
