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

import sys
from time import ctime, time

from smartcard.pcsc.PCSCPart10 import (
    SCARD_CTL_CODE,
    SCARD_LEAVE_CARD,
    SCARD_SHARE_DIRECT,
)
from smartcard.System import readers


def stress(reader):
    """stress"""
    print("Using:", reader)
    cardConnection = reader.createConnection()
    cardConnection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

    get_firmware = [0x02]
    IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)
    cpt = 0
    while True:
        before = time()
        res = cardConnection.control(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE, get_firmware)
        after = time()
        delta = after - before
        print(f"{cpt} Reader: {reader}, delta: {delta}")
        print("Firmware:", "".join([chr(x) for x in res]))
        if delta > 1:
            sys.stderr.write(ctime() + f" {delta}\n")
        cpt += 1


if __name__ == "__main__":
    # get all the available readers
    readers = readers()
    print("Available readers:")
    for i, r in enumerate(readers):
        print(f"{i}: {r}")

    try:
        i = int(sys.argv[1])
    except IndexError:
        i = 0

    stress(readers[i])
