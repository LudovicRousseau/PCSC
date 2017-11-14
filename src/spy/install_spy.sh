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

# find the spying library
SPY=$(ldconfig --print-cache | grep libpcscspy.so)
SPY=$(echo $SPY | cut -d'=' -f2 | cut -d' ' -f2)

echo "Using directory:" $DIR
echo "Spying library is:" $SPY

cd $DIR

NOSPY=libpcsclite_nospy.so.1

if [ -f $NOSPY ]
then
	echo "File $NOSPY already exists"
else
	# backup the real library
	cp libpcsclite.so.1 $NOSPY
fi

# link to the spy library
ln -sf $SPY libpcsclite.so.1.0.0
ln -sf libpcsclite.so.1.0.0 libpcsclite.so.1
