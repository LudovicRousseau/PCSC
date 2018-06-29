#!/bin/sh

set -e
set -x

rm -rf api
make doxygen
rsync --recursive --verbose --update --rsh=ssh api pcsclite.apdu.fr:Serveurs_web/pcsclite.apdu.fr/
