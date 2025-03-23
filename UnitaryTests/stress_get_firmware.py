#! /usr/bin/env python3

"""
stress_get_firmware.py: get firmware version of Gemalto readers in loop
Copyright (C) 2010  Ludovic Rousseau
"""

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, see <http://www.gnu.org/licenses/>.

from time import time, ctime
from smartcard.System import readers
from smartcard.pcsc.PCSCPart10 import (
    SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD,
    SCARD_CTL_CODE,
)


def stress(reader):
    cardConnection = reader.createConnection()
    cardConnection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

    get_firmware = [0x02]
    IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)
    i = 0
    while True:
        before = time()
        res = cardConnection.control(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE, get_firmware)
        after = time()
        delta = after - before
        print("%d Reader: %s, delta: %d" % (i, reader, delta))
        print("Firmware:", "".join([chr(x) for x in res]))
        if delta > 1:
            sys.stderr.write(ctime() + " %f\n" % delta)
        i += 1


if __name__ == "__main__":
    import sys

    # get all the available readers
    readers = readers()
    print("Available readers:")
    i = 0
    for r in readers:
        print("%d: %s" % (i, r))
        i += 1

    try:
        i = int(sys.argv[1])
    except IndexError:
        i = 0

    reader = readers[i]
    print("Using:", reader)

    stress(reader)
