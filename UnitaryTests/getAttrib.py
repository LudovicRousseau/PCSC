#! /usr/bin/env python3

"""
#   getAttrib.py : Unitary test for SCardGetAttrib()
#   Copyright (C) 2011  Ludovic Rousseau
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

from smartcard.System import readers
from smartcard.scard import (SCARD_ATTR_VENDOR_NAME, SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD, SCARD_ATTR_DEVICE_FRIENDLY_NAME,
    SCARD_ATTR_VENDOR_IFD_VERSION, SCARD_ATTR_VENDOR_IFD_SERIAL_NO)
import smartcard.Exceptions


def main():
    """ Ask the first reader/driver to return its Vendor Name if any """
    card_connection = readers()[0].createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT,
        disposition=SCARD_LEAVE_CARD)

    try:
        # Vendor name
        name = card_connection.getAttrib(SCARD_ATTR_VENDOR_NAME)
        print(''.join([chr(char) for char in name]))

        # Vendor-supplied interface device version (DWORD in the form
        # 0xMMmmbbbb where MM = major version, mm = minor version, and
        # bbbb = build number).
        version = card_connection.getAttrib(SCARD_ATTR_VENDOR_IFD_VERSION)
        print("Version: %d.%d.%d" % (version[3], version[2],
                version[0]))

        # Vendor-supplied interface device serial number.
        # only for readers with a USB serial number
        serial = card_connection.getAttrib(SCARD_ATTR_VENDOR_IFD_SERIAL_NO)
        print(serial)

        # Reader's display name
        # only with pcsc-lite version >= 1.6.0
        name = card_connection.getAttrib(SCARD_ATTR_DEVICE_FRIENDLY_NAME)
        print(''.join([chr(char) for char in name]))

    except smartcard.Exceptions.SmartcardException as ex:
        print("Exception:", ex)

if __name__ == "__main__":
    main()
