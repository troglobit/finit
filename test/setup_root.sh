#!/bin/sh

set -eu

# shellcheck disable=SC2154
"$srcdir"/../configure --prefix=/
make
make DESTDIR="$(pwd)/test_root/" install

mkdir -p test_root/bin
cp "${srcdir}/test_root/bin/chrootsetup.sh" test_root/bin
cp "${srcdir}/test_root/busybox-x86_64.md5" test_root/
cp "${srcdir}/test_root/Makefile" test_root/

make -C test_root/
