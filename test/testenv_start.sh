#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")
TESTENV_ROOT="${TESTENV_ROOT:-$(pwd)/${TEST_DIR}/testenv-root}"

unshare=$(command -v unshare)
chroot=$(command -v chroot)

export PS1='\w \$ '
export PS2='> '
export PS3='#? '
export PS4='+ '

PATH="$TESTENV_PATH"
export PATH

# Not supported by Busybox unshare:
#  --cgroup --time
exec "$unshare" \
    --user --map-root-user \
    --fork --pid --mount-proc \
    --mount \
    --mount-proc \
    --uts --ipc --net \
    "$chroot" "$TESTENV_ROOT" /bin/chrootsetup.sh "$@"
