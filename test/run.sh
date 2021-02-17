#!/bin/sh

set -eu

TEST_DIR=$(dirname "$0")
TESTS_ROOT="$(pwd)/${TEST_DIR}/test_root"

echo "Hint: Execute './test/testenter.sh \$(pgrep -P $$) /bin/sh; reset' to enter the test namespace"

# Not supported by Busybox unshare:
#  --cgroup --time
export PATH=/sbin:/bin
exec unshare \
    --user --map-root-user \
    --fork --pid --mount-proc \
    --mount \
    --mount-proc \
    --uts --ipc --net \
    chroot "$TESTS_ROOT" /bin/testwrapper.sh "$@"
