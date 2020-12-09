#!/bin/sh
USAGE="Usage: $0 processName"
LOG_FILE="/tmp/memusage.csv"
ELAPSED_TIME=0
PERIOD=1        # seconds

if test $# -ne 1; then
        echo $USAGE
        exit 1
fi

echo "ElapsedTime,VmSize,VmRSS" > $LOG_FILE

# Monitor memory usage forever until script is killed
while :
do
        SUM_VM_SIZE=0
        SUM_RSS_SIZE=0
        # In case the monitored process has not yet started
        # keep searching until its PID is found
        PROCESS_PIDS=""
        while :
        do
                PROCESS_PIDS=`/bin/pidof $1`
                if test "$PROCESS_PIDS.X" != ".X"; then
                        break
                fi
        done

        for PID in ${PROCESS_PIDS} ; do
                VM_SIZE=`awk '/VmSize/ {print $2}' < /proc/$PID/status`
                if test "$VM_SIZE.X" = ".X"; then
                        continue
                fi
                #echo exprVM_ $SUM_VM_SIZE + $VM_SIZE
                SUM_VM_SIZE=`expr $SUM_VM_SIZE + $VM_SIZE`

                VM_RSS=`awk '/VmRSS/ {print $2}' < /proc/$PID/status`
                if test "$VM_RSS.X" = ".X"; then
                        continue
                fi
                SUM_RSS_SIZE=`expr $SUM_RSS_SIZE + $VM_RSS`
        done
        echo "$ELAPSED_TIME sec, $SUM_VM_SIZE KB, $SUM_RSS_SIZE KB"
        echo "$ELAPSED_TIME,$SUM_VM_SIZE,$SUM_RSS_SIZE" >> $LOG_FILE
        sleep $PERIOD
        VM_SIZE=""
        VM_RSS=""
        # Needs to get actual elapsed time instead of doing this
        ELAPSED_TIME=`expr $ELAPSED_TIME + $PERIOD`
done
