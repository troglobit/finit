#!/bin/sh
# Called by kernel when last child in cgroup exists

BASE=/sys/fs/cgroup/finit
PROC=`basename $1`
DIR="$BASE$1"

#/libexec/finit/logit -p daemon.info -t finit "$PROC stopped, cleaning up control group $DIR"
rmdir $DIR

exit 0
