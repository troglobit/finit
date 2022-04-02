#!/bin/sh
# SysV style start script for service.sh

DAEMON="/test_assets/service.sh"
PIDFILE="/var/run/service.pid"

start()
{
	printf 'Starting %s: ' "$DAEMON"
	start-stop-daemon -S -b -q -p "$PIDFILE" -x "$DAEMON"

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
