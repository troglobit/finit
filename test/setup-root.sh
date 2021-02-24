#!/bin/sh

set -eu

# shellcheck disable=SC2154
if [ ! -f "$srcdir"/../config.status ]; then
    "$srcdir"/../configure --prefix=/
    make
    make DESTDIR="$(pwd)/test-root/" install

    mkdir -p test-root/bin
    cp "${srcdir}/test-root/bin/chrootsetup.sh" test-root/bin
    cp "${srcdir}/test-root/busybox-x86_64.md5" test-root/
    cp "${srcdir}/test-root/Makefile" test-root/
    make -C test-root/
    config_h='./config.h'
    echo "export TESTENV_PATH=/bin:/sbin" > test.env
else
    make -C ../ DESTDIR="$(pwd)/test-root/" install
    FINITBIN=../../src/finit make -C test-root/
    config_h='../config.h'
    echo "export TESTENV_PATH=/bin:/sbin:$PREFIX/sbin:$PREFIX/bin" > test.env
fi

echo "export FINIT_CONF=$(grep FINIT_CONF $config_h | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
echo "export FINIT_RCSD=$(grep FINIT_RCSD $config_h | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
