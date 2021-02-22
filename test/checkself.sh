#!/bin/sh

set -eux

# shellcheck disable=SC2046
shellcheck $(find ./ -name '*.sh')
