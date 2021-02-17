#!/bin/sh

set -eu

mount -n -t tmpfs none /dev
# mount -t devtmpfs none /dev

# mknod -m 622 /dev/console c 5 1
# mknod -m 666 /dev/null c 1 3
# mknod -m 666 /dev/zero c 1 5
# mknod -m 666 /dev/ptmx c 5 2
# mknod -m 666 /dev/tty c 5 0
# mknod -m 444 /dev/random c 1 8
# mknod -m 444 /dev/urandom c 1 9
# chown root:tty /dev/{console,ptmx,tty}

mount -t proc none /proc
mount -t sysfs none /sys
mount -t tmpfs none /tmp

# mkdir -p /dev/pts
# mount -t devpts none /dev/pts

tty=/dev/$(cat /sys/class/tty/console/active)
mkfifo "$tty"

exec "$@"
