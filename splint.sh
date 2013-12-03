#!/bin/sh

set -e

# do not use debug.c and debuglog.c, they #include <syslog.h>
# and splint does not like this include
# do not use configfile.c since it is a lex file from configfile.l
if [ $# -lt 1 ]
then
	files=$(ls -1 src/*.c | grep -v debug | grep -v configfile)
else
	files="$@"
fi
inc="-I. -Isrc -Isrc/PCSC -I/usr/include/hal -I/usr/include/dbus-1.0
-I/usr/lib/dbus-1.0/include -I/usr/include/x86_64-linux-gnu "
opt="-warnposix -unrecog -type -predboolint -likelybool -preproc"

splint $inc $opt $files
