#!/bin/sh
# SysV style start script for serv

DAEMON="/sbin/serv"
PIDFILE="/var/run/serv.pid"

# shellcheck source=/dev/null
[ -r "/etc/default/serv" ] && . "/etc/default/serv"

start()
{
	printf 'Starting %s: ' "$DAEMON $SERV_ARGS"
	# shellcheck disable=SC2086 # we need the word splitting
	start-stop-daemon -S -b -q -p "$PIDFILE" -x "$DAEMON" -- $SERV_ARGS

	status=$?
	if [ "$status" -eq 0 ]; then
		echo "OK"
	else
		echo "FAIL"
	fi

	return "$status"
}

stop()
{
	printf 'Stopping %s: ' "$DAEMON"
	start-stop-daemon -K -q -p "$PIDFILE"

	status=$?
	if [ "$status" -eq 0 ]; then
		echo "OK"
	else
		echo "FAIL"
	fi

	return "$status"
}

restart()
{
	stop
	sleep 1
	start
}

case "$1" in
	start|stop|restart)
		"$1"
		;;
	*)
		echo "Usage: $0 {start|stop|restart}, got $0 $1"
		exit 1
esac
