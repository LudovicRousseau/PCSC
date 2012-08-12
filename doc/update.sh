#!/bin/sh

set -e
set -x

rm -rf api
make doxygen
rsync --recursive --verbose --update --rsh=ssh api anonscm.debian.org:pcsclite_htdocs/
