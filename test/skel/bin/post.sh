#!/bin/sh

marker="POST"
# shellcheck disable=SC2154
if [ -n "$baz" ]; then
    marker=$baz
fi

echo "$marker"                   > /tmp/post
echo "EXIT_CODE=$EXIT_CODE"     >> /tmp/post
echo "EXIT_STATUS=$EXIT_STATUS" >> /tmp/post
