#!/bin/sh

set -eu

echo $$ > /run/service.pid

while true; do
  sleep 5
done

rm /run/service.pid
