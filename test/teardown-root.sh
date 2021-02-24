#!/bin/sh

set -eu

if [ -d test-root/var/lock ]; then
    chmod +r test-root/var/lock
fi
make -C test-root/ clean
rm -f test.env
