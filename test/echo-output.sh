#!/bin/sh

echo 'stderr line1' > /dev/stderr
echo 'stderr line2' > /dev/stderr

echo 'stdout line1' > /dev/stdout
echo 'stdout line2' > /dev/stdout

touch /tmp/test-cron-echo-output.txt

