#!/bin/sh

cd /usr/lib

if [ -f libpcsclite_nospy.so.1 ]
then
	echo "File libpcsclite_nospy.so.1 already exists"
	exit
fi

# backup the real library
cp libpcsclite.so.1 libpcsclite_nospy.so.1

# link to the spy library
ln -sf libpcscspy.so.0.0.0 libpcsclite.so.1.0.0
