#!/bin/sh

set -x

./configure \
        --prefix=/usr \
        --sysconfdir=/etc \
        --enable-maintainer-mode \
        --enable-twinserial \
        CFLAGS="$CFLAGS" \
        "$@"
