#!/bin/sh

set -e
set -x

cd ../builddir
rm -rf doc

meson compile doc

rsync --recursive --verbose --update --rsh=ssh doc/api pcsclite.apdu.fr:Serveurs_web/pcsclite.apdu.fr/
