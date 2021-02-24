#!/bin/sh

set -eu

if [ "$#" -lt 2 ]; then
    echo "Usage:"
    echo "  $0 [target-pid] [command [arguments]]"
    exit 1
fi
target="$1"
shift

TEST_DIR=$(dirname "$0")
TESTS_ROOT="${TESTS_ROOT:-$(pwd)/${TEST_DIR}/test_root}"

nsenter=$(command -v nsenter)
chroot=$(command -v chroot)

export PS1='\w \$ '
export PS2='> '
export PS3='#? '
export PS4='+ '

"$nsenter" \
    --preserve-credentials \
    --user \
    --mount \
    --uts \
    --ipc \
    --net \
    --pid \
    -w -t "$target" \
    "$chroot" "$TESTS_ROOT" "$@"
