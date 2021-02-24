#!/bin/sh

set -eu

if [ -d test_root/var/lock ]; then
    chmod +r test_root/var/lock
fi
make -C test_root/ clean
rm -f test.env
