#!/bin/sh

set -eux

TEST_DIR=$(dirname "$0")
TESTS_ROOT="${TEST_DIR}/test_root"

cp "${TEST_DIR}/testwrapper.sh" "${TESTS_ROOT}/usr/local/bin/"

unshare \
    --user --map-root-user \
    --fork --pid --mount-proc \
    --mount \
    --mount-proc \
    chroot "$TESTS_ROOT" /usr/local/bin/testwrapper.sh "$@"
