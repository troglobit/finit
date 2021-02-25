#!/bin/sh

set -eu

if [ ! -d test-root ]; then
    mkdir -p test-root/bin
    # shellcheck disable=SC2154
    cp "${srcdir}/test-root/bin/chrootsetup.sh" test-root/bin
    cp "${srcdir}/test-root/busybox-x86_64.md5" test-root/
    cp "${srcdir}/test-root/Makefile" test-root/
fi

# shellcheck disable=SC2154
make -C "$top_builddir" DESTDIR="$(pwd)/test-root/" install
FINITBIN="$(pwd)/$top_builddir/src/finit" make -C test-root/
