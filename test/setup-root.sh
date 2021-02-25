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
FINITBIN="$(pwd)/$top_builddir/src/finit" make -C test-root/  # DESTDIR="$(pwd)/test-root/"
config_h="$top_builddir/config.h"
echo "export TESTENV_PATH=/bin:/sbin:$PREFIX/sbin:$PREFIX/bin" > test.env

echo "export FINIT_CONF=$(grep FINIT_CONF "$config_h" | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
echo "export FINIT_RCSD=$(grep FINIT_RCSD "$config_h" | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
