#!/bin/sh

set -eu

# It would have been nice to mount /dev as devtmpfs, but unfortunately
# that's not possible if you're not privileged. For now tmpfs will
# have to do.
mount -n -t tmpfs none /dev

mount -t proc none /proc
mount -t sysfs none /sys

mkdir -p /tmp.shadow
mount --bind /tmp /tmp.shadow
mount -t tmpfs none /tmp
cp -a /tmp.shadow/* /tmp/

mkdir -p "$FINIT_RCSD"

tty=/dev/$(cat /sys/class/tty/console/active)
mkfifo "$tty"

exec "$@"
