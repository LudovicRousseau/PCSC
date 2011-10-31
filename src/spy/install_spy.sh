#!/bin/sh

# exit on the first error
set -e

cd /usr/lib

NOSPY=libpcsclite_nospy.so.1

if [ -f $NOSPY ]
then
	echo "File $NOSPY already exists"
else
	# backup the real library
	cp libpcsclite.so.1 $NOSPY
fi

# link to the spy library
ln -sf libpcscspy.so.0.0.0 libpcsclite.so.1.0.0
ln -sf libpcsclite.so.1.0.0 libpcsclite.so.1
