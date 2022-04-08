#!/bin/sh

nm=$1

# shellcheck disable=SC2046
getpid()
{
	initctl status "$1" |awk '/PID :/{ print $3; }'
}

pid=$(getpid "$nm")
if [ -f oldpid ]; then
	oldpid=$(cat oldpid)
	if [ "$oldpid" = "$pid" ]; then
		echo "Looks bad, wait for it ..."
		sleep 3
		pid=$(getpid "$nm")
		if [ "$oldpid" = "$pid" ]; then
			echo "Finit did not deregister old PID $oldpid vs $pid"
			initctl status "$nm"
			ps
			echo "Reloading finit ..."
			initctl reload
			sleep 1
			initctl status "$nm"
			exit 1
		fi
	fi
fi

if [ "$pid" -le 1 ]; then
	sleep 3
	pid=$(getpid "$nm")
	if [ "$pid" -le 1 ]; then
		echo "Got a bad PID: $pid, aborting ..."
		ps
		sleep 1
		initctl status "$nm"
		ps
		exit 1
	fi
fi

echo "$pid" > oldpid

#echo "PID $pid, kill -9 ..."
kill -9 "$pid"
