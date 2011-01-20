#!/bin/sh

set -e

# do not use configfile.c since it is a lex file from configfile.l
if [ $# -lt 1 ]
then
	files=$(ls -1 src/*.c | grep -v configfile | grep -v tokenparser)
else
	files="$@"
fi
inc="-I. -Isrc -Isrc/PCSC -I/usr/include/hal -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include "
opt="--analyze "

clang $inc $opt $files
