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

assert_num_services()
{
	assert "$1 services are loaded" "$(texec initctl -t status "$2" | wc -l)" -eq "$1"
}

assert_forking()
{
	assert "service $1 forking:$2" "$(texec initctl -j status "$1" | jq -M .forking)" = "$2"
}

assert_nopid()
{
	assert "service $1 pid:0" "$(texec initctl -j status "$1" | jq -M .pid)" -eq 0
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

assert_desc()
{
	assert "Service description == $1" "$(texec initctl status "$2" | grep 'Description' | sed 's/Description : //')" = "$1"
}

assert_status()
{
	service=$1
	status=$2
	assert "Service $service $status" "$(texec initctl -p status "$service" | awk '/Status/{print $3}')" = "$status"
}

assert_cond()
{
	cond=$1
	assert "Condition $cond asserted" "$(texec initctl -v cond get "$cond")" = "on"
}

assert_nocond()
{
	cond=$1
	assert "Condition $cond cleared" "$(texec initctl -v cond get "$cond")" = "off"
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

run()
{
	texec sh -c "$@"
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

say()
{
	log "$fg_yellow" • "$@"
}

skip()
{
	log "$fg_yellow" − "$*"
	exit 77
}

fail()
{
	log "$fg_red" ✘ "$*"
	exit 99
}

# shellcheck disable=SC2068
check_dep()
{
	if ! command -v "$1"; then
		skip "Cannot find $1, skipping test."
	fi
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
	return 0
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

teardown()
{
	test_status="$?"

	if type test_teardown > /dev/null 2>&1 ; then
		test_teardown
	fi

	log "$color_reset" '--' ''
	if [ "$test_status" -eq 0 ]; then
		log "$fg_green" 'TEST PASS' ''
	elif [ "$test_status" -eq 77 ]; then
		log "$fg_yellow" 'TEST SKIP' ''
	else
		log "$fg_red" 'TEST FAIL' ''
	fi
	if [ -n "${finit_pid+x}" ]; then
		texec kill -SIGUSR2 1
	fi

	wait

	rm -f "$TENV_ROOT"/running_test.pid
	exit $test_status
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

# Tests may want to override runlevel
if [ -n "${FINIT_RUNLEVEL:-}" ]; then
	FINIT_ARGS="${FINIT_ARGS:-} $FINIT_RUNLEVEL"
else
	FINIT_RUNLEVEL=2
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
	lvl=$(texec sh -c "initctl runlevel | awk '{print \$2;}'")
	if [ "$lvl" -eq "$FINIT_RUNLEVEL" ]; then
#		say "Reached runlevel $FINIT_RUNLEVEL, releasing test."
		break;
	fi
	i=$((i + 1))
	sleep 1
done

if type test_setup > /dev/null 2>&1 ; then
	test_setup
fi
