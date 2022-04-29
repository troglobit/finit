#!/bin/sh

marker="PRE"
# shellcheck disable=SC2154
if [ -n "$foo" ]; then
    marker=$foo
fi

echo "$marker" > /tmp/pre
