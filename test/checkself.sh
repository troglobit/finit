#!/bin/sh

set -eu

testdir="${srcdir:-$(dirname "$0")}"

# shellcheck disable=SC2046
shellcheck $(find "$testdir" -path "$testdir/test-root" -prune -false -o -name '*.sh')
