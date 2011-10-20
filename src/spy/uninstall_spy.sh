#!/bin/sh

# exit on the first error
set -e

cd /usr/lib

# Use the real library again
mv libpcsclite_nospy.so.1 libpcsclite.so.1.0.0
