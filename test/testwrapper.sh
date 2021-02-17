#!/bin/sh

set -eux

export PATH=/sbin:/bin

mount -t proc none /proc

exec "$@"
