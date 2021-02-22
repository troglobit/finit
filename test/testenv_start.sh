#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")
TESTS_ROOT="$(pwd)/${TEST_DIR}/test_root"

>&2 echo "Hint: Execute '${TEST_DIR}/testenv_exec.sh \$(pgrep -P $$) /bin/sh; reset' to enter the test namespace"

unshare=$(command -v unshare)
chroot=$(command -v chroot)

export PATH=/sbin:/bin
export PS1='\w \$ '
export PS2='> '
export PS3='#? '
export PS4='+ '

# Not supported by Busybox unshare:
#  --cgroup --time
exec "$unshare" \
    --user --map-root-user \
    --fork --pid --mount-proc \
    --mount \
    --mount-proc \
    --uts --ipc --net \
    "$chroot" "$TESTS_ROOT" /bin/chrootsetup.sh "$@"
