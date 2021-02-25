#!/bin/sh

set -eu

make -C test-root/ clean
rm -f test.env
