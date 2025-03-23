#! /usr/bin/env python3

"""
#   control_switch_interface.py: switch interface on the GemProx DU
#   Copyright (C) 2009-2010  Ludovic Rousseau
"""
#
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

from smartcard.System import readers
from smartcard.pcsc.PCSCPart10 import (
    SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD,
    SCARD_CTL_CODE,
)
from smartcard.Exceptions import SmartcardException

IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)


def switch_interface(interface):
    """
    switch from contact to contactless (or reverse) on a GemProx DU reader
    """
    for reader in readers():
        cardConnection = reader.createConnection()
        cardConnection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

        switch_interface_cmd = [0x52, 0xF8, 0x04, 0x01, 0x00, interface]
        print("Reader:", reader, "=>", end=" ")
        try:
            res = cardConnection.control(
                IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE, switch_interface_cmd
            )
        except SmartcardException as e:
            print("FAILED")
        else:
            if res != [0, 0, 0, 0]:
                print("Failed: ", end="")
                err = res[0] * 256 + res[1]
                if err == 0xFF83:
                    print("Wrong data parameters")
                elif err == 0xFF84:
                    print("Wrong command bytes")
                else:
                    print("Unknown error:", [hex(x) for x in res])
            else:
                print("Success")


if __name__ == "__main__":
    import sys

    # 01h = Switch to contactless interface
    # 02h = Switch to contact interface

    # switch to contactless by default
    interface = 0x01
    if len(sys.argv) > 1:
        interface = int(sys.argv[1])

    switch_interface(interface)
