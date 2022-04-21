#!/bin/sh

num=$(cat /run/task.state)
if [ -z "$num" ]; then
   num=0
fi

num=$((num + 1))
echo $num > /run/task.state
