#!/bin/sh

set -eu

if [ "$#" -lt 2 ]; then
    echo "Usage:"
    echo "  $0 [target-pid] [command [arguments]]"
    exit 1
fi
target="$1"
shift

TEST_DIR=$(dirname "$0"../)
TENV_ROOT="${TENV_ROOT:-$(pwd)/${TEST_DIR}/tenv-root}"

nsenter=$(command -v nsenter)
chroot=$(command -v chroot)

export PS1='\w \$ '
export PS2='> '
export PS3='#? '
export PS4='+ '

PATH="$TESTENV_PATH"
export PATH

"$nsenter" \
    --preserve-credentials \
    --user \
    --mount \
    --uts \
    --ipc \
    --net \
    --pid \
    -w -t "$target" \
    "$chroot" "$TENV_ROOT" "$@"
