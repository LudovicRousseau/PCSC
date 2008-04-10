#!/bin/sh

set -x

CFLAGS="$CFLAGS -Wall -g -D_REENTRANT -O2 -Wextra -Wno-sign-compare"
CFLAGS="$CFLAGS -pipe -funsigned-char -fstrict-aliasing -Wchar-subscripts -Wundef -Wshadow -Wcast-align -Wwrite-strings -Wsign-compare -Wunused -Wno-unused-value -Wuninitialized -Wpointer-arith -Wredundant-decls -Wmissing-prototypes"
CFLAGS="$CFLAGS -Wstrict-prototypes -Wold-style-definition -Wmissing-declarations"
CFLAGS="$CFLAGS -Wno-unused-parameter"

./configure \
        --prefix=/usr \
        --sysconfdir=/etc \
        --enable-usbdropdir=/usr/lib/pcsc/drivers \
        --enable-muscledropdir=/usr/lib/pcsc/services \
        --enable-maintainer-mode \
        --enable-twinserial \
        CFLAGS="$CFLAGS" \
        "$@"
