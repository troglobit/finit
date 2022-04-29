#!/bin/sh

marker="POST"
# shellcheck disable=SC2154
if [ -n "$baz" ]; then
    marker=$baz
fi

echo "$marker" > /tmp/post
