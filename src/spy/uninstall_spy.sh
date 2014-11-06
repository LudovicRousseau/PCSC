#!/bin/sh

# exit on the first error
set -e

# ldconfig --print-cache will return something like
# libpcsclite.so.1 (libc6,x86-64) => /usr/lib/x86_64-linux-gnu/libpcsclite.so.1
DIR=$(ldconfig --print-cache | grep libpcsclite.so.1)

# get the right part only: /usr/lib/x86_64-linux-gnu/libpcsclite.so.1
DIR=$(echo $DIR | cut -d'=' -f2 | cut -d' ' -f2)

# get the directory part only: /usr/lib/x86_64-linux-gnu
DIR=$(dirname $DIR)

echo "Using directory:" $DIR

cd $DIR

# Use the real library again
mv libpcsclite_nospy.so.1 libpcsclite.so.1.0.0
