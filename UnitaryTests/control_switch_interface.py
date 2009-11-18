#! /usr/bin/env python

#   control_switch_interface.py: switch interface on the GemProx DU
#   Copyright (C) 2009  Ludovic Rousseau
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

from smartcard.pcsc.PCSCReader import readers
from smartcard.pcsc.PCSCPart10 import *
from smartcard.util import toHexString


def switch_interface(interface):
    for reader in readers():
        cardConnection = reader.createConnection()
        cardConnection.connect(mode=SCARD_SHARE_DIRECT)

        switch_interface = [0x52, 0xF8, 0x04, 0x01, 0x00, interface]
        IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)
        print "Reader:", reader,
        try:
            res = cardConnection.control(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE,
                switch_interface)
        except:
            print "FAILED"
        else:
            print res

if __name__ == "__main__":
    import sys

    # switch to contactless by default
    # use 2 as argument on the command line to switch to contact
    interface = 0x01
    if len(sys.argv) > 1:
        interface = int(sys.argv[1])

    switch_interface(interface)
