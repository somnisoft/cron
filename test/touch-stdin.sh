#!/bin/sh

while read line
do
  touch "${line}"
  if [ $? != 0 ] ; then
    echo "Failed to create file: ${line}" > /dev/stderr
  fi
done <&0

