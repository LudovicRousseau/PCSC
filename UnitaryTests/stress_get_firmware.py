#! /usr/bin/env python

"""
    stress_get_firmware.py: get firmware version of Gemalto readers in loop
    Copyright (C) 2010  Ludovic Rousseau
"""

#   this program is free software; you can redistribute it and/or modify
#   it under the terms of the gnu general public license as published by
#   the free software foundation; either version 2 of the license, or
#   (at your option) any later version.
#
#   this program is distributed in the hope that it will be useful,
#   but without any warranty; without even the implied warranty of
#   merchantability or fitness for a particular purpose.  see the
#   gnu general public license for more details.
#
#   you should have received a copy of the gnu general public license along
#   with this program; if not, write to the free software foundation, inc.,
#   51 franklin street, fifth floor, boston, ma 02110-1301 usa.

from smartcard.pcsc.PCSCReader import readers
from smartcard.pcsc.PCSCPart10 import (SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD, SCARD_CTL_CODE)


def stress(reader):
    cardConnection = reader.createConnection()
    cardConnection.connect(mode=SCARD_SHARE_DIRECT,
        disposition=SCARD_LEAVE_CARD)

    get_firmware = [0x02]
    IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)
    i = 0
    while True:
        res = cardConnection.control(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE,
            get_firmware)
        print "%d Reader: %s" % (i, reader)
        print "Firmware:", "".join([chr(x) for x in res])
        i += 1

if __name__ == "__main__":
    import sys

    # get all the available readers
    readers = readers()
    print "Available readers:", readers

    try:
        i = int(sys.argv[1])
    except IndexError:
        i = 0

    reader = readers[i]
    print "Using:", reader

    stress(reader)
