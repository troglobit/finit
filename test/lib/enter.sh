#!/bin/sh

set -eu

cmd=""
if [ "$#" -lt 1 ]; then
    cmd="/bin/sh"
else
    cmd=$1
    shift
fi

TEST_DIR=$(dirname "$0")/..
SYSROOT="${SYSROOT:-$(pwd)/${TEST_DIR}/sysroot}"
PID_FILE="$SYSROOT/running_test.pid"

if [ -f  "$PID_FILE" ] ; then
    target=$(cat "$PID_FILE")
    target=$(pgrep -P "$target")
else
    echo "No test running!"
    exit 1
fi

# shellcheck disable=SC1091
. "$TEST_DIR/test.env"

"$(dirname "$0")"/exec.sh "$target" "$cmd" "$@"
