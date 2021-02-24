#!/bin/sh

set -eu

cmd=""
if [ "$#" -lt 1 ]; then
    cmd="/bin/sh"
else
    cmd=$1
    shift
fi

PID_FILE=$(dirname "$0")/test-root/running_test.pid

if [ -f  "$PID_FILE" ] ; then
    target=$(cat "$PID_FILE")
    target=$(pgrep -P "$target")
else
    echo "No test running!"
    exit 1
fi

PATH="$TESTENV_PATH"
export PATH

"$(dirname "$0")"/testenv_exec.sh "$target" "$cmd" "$@"
