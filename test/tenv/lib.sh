#!/bin/sh
# Small framework for executing tests in a test environment.
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

assert_file_contains()
{
	assert "File $1 contains the string $2" "$(texec grep "$2" "$1")"
}

assert_num_children()
{
	assert "$1 services are running" "$(texec pgrep -P 1 "$2" | wc -l)" -eq "$1"
}

# shellcheck disable=SC2086
assert_new_pid()
{
	assert "Finit has registered new PID" "$(texec initctl |grep $1 | awk '{print $1;}')" -eq "$(texec cat $2)"
}

assert_restarts()
{
	assert "Finit has registered restarts" "$(texec initctl status "$2" | awk '/Restarts/{print $3;}')" -ge "$1"
}

# shellcheck disable=SC2086
assert_pidfile()
{
	assert "Process has PID file" "$(texec find $1 2>/dev/null)"
}

# shellcheck disable=SC2154
texec()
{
	"$TEST_DIR/tenv/exec.sh" "$finit_pid" "$@"
}

pause()
{
	echo "Press any key to continue... "
	read -r REPLY
}

toggle_finit_debug()
{
	say 'Toggle finit debug'
	texec initctl debug
	sleep 0.5
}

color_reset='\e[0m'
fg_red='\e[1;31m'
fg_green='\e[1;32m'
fg_yellow='\e[1;33m'
log()
{
	test=$(basename "$0" ".sh")
	printf "\e[2m[%s]\e[0m %b%b%b %s\n" "$test" "$1" "$2" "$color_reset" "$3"
}

assert()
{
	__assert_msg=$1
	shift

	if [ ! "$@" ]; then
		log "$fg_red" ✘ "$__assert_msg ($*)"
		return 1
	fi

	log "$fg_green" ✔ "$__assert_msg"
}

retry()
{
	__retry_cmd=$1
	shift

	case "$#" in
		2)
			__retry_n=$1
			__retry_sleep=$2
			;;
		1)
			__retry_n=$1
			__retry_sleep=0.1
			;;
		*)
			__retry_n=50
			__retry_sleep=0.1
			;;
	esac
	
	for _ in $(seq 1 "$__retry_n"); do
		sleep "$__retry_sleep"
		__retry_cmd_out=$(eval "$__retry_cmd") && \
			echo "$__retry_cmd_out" && \
			return 0
	done

	__retry_cmd_exit="$?"
	echo "$__retry_cmd_out"

	return "$__retry_cmd_exit"
}

say()
{
	log "$fg_yellow" • "$@"
}

teardown()
{
	test_status="$?"

	if type test_teardown > /dev/null 2>&1 ; then
		test_teardown
	fi

	log "$color_reset" '--' ''
	if [ "$test_status" -eq 0 ]; then
		log "$fg_green" 'TEST PASS' ''
	else
		log "$fg_red" 'TEST FAIL' ''
	fi
	if [ -n "${finit_pid+x}" ]; then
		texec kill -SIGUSR2 1
	fi

	wait

	rm -f "$TENV_ROOT"/running_test.pid
}

trap teardown EXIT

TENV_ROOT="${TENV_ROOT:-$(pwd)/${TEST_DIR}/tenv-root}"
export TENV_ROOT

# shellcheck source=/dev/null
. "$TENV_ROOT/../test.env"

# Setup test environment
if [ -n "${DEBUG:-}" ]; then
	FINIT_ARGS="${FINIT_ARGS:-} finit.debug=on"
fi
# shellcheck disable=2086
"$TEST_DIR/tenv/start.sh" finit ${FINIT_ARGS:-} &
finit_ppid=$!
echo "$finit_ppid" > "$TENV_ROOT"/running_test.pid

#>&2 echo "Hint: Execute 'TENV_ROOT=$TENV_ROOT $TEST_DIR/tenv/enter.sh' to enter the test namespace"
log "$color_reset" 'Setup of test environment done, waiting for Finit ...' ''

finit_pid=$(retry "pgrep -P $finit_ppid")

#tty=/dev/$(texec cat /sys/class/tty/console/active)
#texec cat "$tty" &

# Allow Finit to start up properly before launching the test
i=0
while [ $i -lt 10 ]; do
	echo "Checking runlevel ..."
	lvl=$(texec sh -c "initctl runlevel | awk '{print \$2;}'")
	echo "Runlevel is $lvl"
	if [ $lvl -eq 2 ]; then
		say 'Reached runlevel 2, releasing test.'
		break;
	fi
	echo "Waiting for Finit ..."
	i=$((i + 1))
	sleep 1
done

if type test_setup > /dev/null 2>&1 ; then
	test_setup
fi
