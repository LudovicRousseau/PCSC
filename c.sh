#!/bin/sh

set -x
set -e

BUILDDIR=builddir
TMPDIR=/tmp/pcsc

rm -rf "$BUILDDIR"

meson setup "$BUILDDIR" \
	--prefix /usr \
	--sbindir usr/sbin \
	-Dsystemdunit=system \
	"$@"

cd "$BUILDDIR"
meson compile

rm -rf "$TMPDIR"
DESTDIR="$TMPDIR" meson install
find "$TMPDIR"
