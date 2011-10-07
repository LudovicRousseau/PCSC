#!/bin/sh

cd /usr/lib

# backup the real library
cp libpcsclite.so.1 libpcsclite_nospy.so.1

# link to the spy library
ln -sf libpcscspy.so.0.0.0 libpcsclite.so.1.0.0
