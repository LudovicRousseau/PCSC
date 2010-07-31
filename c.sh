#!/bin/sh

set -x

#CFLAGS="$CFLAGS -Wall -g -O2 -Wextra -pipe -funsigned-char -fstrict-aliasing -Wchar-subscripts -Wundef -Wshadow -Wcast-align -Wwrite-strings -Wunused -Wno-unused-value -Wuninitialized -Wpointer-arith -Wredundant-decls -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition -Wmissing-declarations -Wbad-function-cast -Winline -Wnested-externs -Wformat-security -Wswitch-enum -Winit-self -Wmissing-include-dirs -Wno-unused-parameter -Wno-sign-compare"

CFLAGS="$CFLAGS -D_REENTRANT"

./configure \
        --prefix=/usr \
        --sysconfdir=/etc \
        --enable-maintainer-mode \
        --enable-twinserial \
        CFLAGS="$CFLAGS" \
        "$@"
