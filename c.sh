#!/bin/sh

set -x

./configure \
        --prefix=/usr \
        --sysconfdir=/etc \
        --enable-maintainer-mode \
        --enable-twinserial \
        --with-systemdsystemunitdir=/lib/systemd/system \
        --enable-usbdropdir=/usr/lib/pcsc/drivers \
        --enable-ipcdir=/run/pcscd \
        --libdir=/usr/lib/x86_64-linux-gnu \
        CFLAGS="$CFLAGS" \
        "$@"
