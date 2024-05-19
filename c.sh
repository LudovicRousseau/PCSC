#!/bin/sh

set -x
set -e

BUILD_DIR=builddir
TMP_DIR=/tmp/pcsc

rm -rf "$BUILD_DIR"

meson setup "$BUILD_DIR" \
	--prefix /usr \
	-Dsystemdunit=system \
	"$@"

cd "$BUILD_DIR"
meson compile

rm -rf "$TMP_DIR"
DESTDIR="$TMP_DIR" meson install
find "$TMP_DIR"
