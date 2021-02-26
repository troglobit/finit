#!/bin/sh

set -eu

# shellcheck disable=SC2154
make -C "$top_builddir" DESTDIR="$TESTENV_ROOT" install
# shellcheck disable=SC2154
FINITBIN="$(pwd)/$top_builddir/src/finit" DEST="$TESTENV_ROOT" make -f "$srcdir/testenv-root.mk"
