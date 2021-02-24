#!/bin/sh

set -eu

# shellcheck disable=SC2154
if [ ! -f "$srcdir"/../config.status ]; then
    "$srcdir"/../configure --prefix=/
    make
    make DESTDIR="$(pwd)/test_root/" install

    mkdir -p test_root/bin
    cp "${srcdir}/test_root/bin/chrootsetup.sh" test_root/bin
    cp "${srcdir}/test_root/busybox-x86_64.md5" test_root/
    cp "${srcdir}/test_root/Makefile" test_root/
    make -C test_root/
    config_h='./config.h'
    echo "export PATH=/bin:/sbin" > test.env
else
    make -C ../ DESTDIR="$(pwd)/test_root/" install
    FINITBIN=../../src/finit make -C test_root/
    config_h='../config.h'
    echo "export PATH=/bin:/sbin:$PREFIX/sbin:$PREFIX/bin" > test.env
fi

echo "export FINIT_CONF=$(grep FINIT_CONF $config_h | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
echo "export FINIT_RCSD=$(grep FINIT_RCSD $config_h | cut -d' ' -f3 | cut -d'"' -f2)" >> test.env
