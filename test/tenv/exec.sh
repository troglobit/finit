#!/bin/sh
# Execute a given command from within the test environment.
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

if [ "$#" -lt 2 ]; then
	echo "Usage:"
	echo "  $0 [target-pid] [command [arguments]]"
	exit 1
fi
target="$1"
shift

TEST_DIR=$(dirname "$0"../)
SYSROOT="${SYSROOT:-$(pwd)/${TEST_DIR}/sysroot}"

nsenter=$(command -v nsenter)
chroot=$(command -v chroot)

export PS1='\w \$ '
export PS2='> '
export PS3='#? '
export PS4='+ '

PATH="$TESTENV_PATH"
export PATH

"$nsenter"					\
	--preserve-credentials			\
	--user					\
	--mount					\
	--uts					\
	--ipc					\
	--net					\
	--pid					\
	-w -t "$target"				\
	"$chroot" "$SYSROOT" "$@"
