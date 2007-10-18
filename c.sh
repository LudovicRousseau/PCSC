#!/bin/sh

CFLAGS="$CFLAGS -Wall -g -D_REENTRANT -O2 -Wextra -Wno-sign-compare"
CFLAGS="$CFLAGS -pipe -funsigned-char -fstrict-aliasing -Wchar-subscripts -Wundef -Wshadow -Wcast-align -Wwrite-strings -Wsign-compare -Wunused -Wno-unused-value -Wuninitialized -Wpointer-arith -Wredundant-decls -Wmissing-prototypes"
CFLAGS="$CFLAGS -Wno-unused-parameter"

./configure \
        --prefix=/usr \
        --enable-usbdropdir=/usr/lib/pcsc/drivers \
        --enable-muscledropdir=/usr/lib/pcsc/services \
        --enable-maintainer-mode \
        CFLAGS="$CFLAGS" \
        "$@"
