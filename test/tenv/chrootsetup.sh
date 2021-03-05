#!/bin/sh
# Setup the things (mount /proc, /tmp, etc.) that needs to be set up from
# within the namespace/chroot and then start PID 1.
#
# Copyright (c) 2021  Jacques de Laval <jacques@de-laval.se>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
#     all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

set -eu

# It would have been nice to mount /dev as devtmpfs, but unfortunately
# that's not possible if you're not privileged. For now tmpfs will
# have to do.
mount -n -t tmpfs none /dev

mount -t proc none /proc
mount -t sysfs none /sys

if [ "$(ls -A /tmp)" ]; then
    mkdir -p /tmp.shadow
    mount --bind /tmp /tmp.shadow
    mount -t tmpfs none /tmp
    cp -a /tmp.shadow/* /tmp/
else
    mount -t tmpfs none /tmp
fi

mkdir -p "$FINIT_RCSD"

tty=/dev/$(cat /sys/class/tty/console/active)
ln -fs /shared/pts "$tty"

exec "$@"
