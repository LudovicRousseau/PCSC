#! /usr/bin/env python

"""
#   getAttrib.py : Unitary test for SCardGetAttrib()
#   Copyright (C) 2011  Ludovic Rousseau
"""

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

from smartcard.System import readers
from smartcard.scard import (SCARD_ATTR_VENDOR_NAME, SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD, SCARD_ATTR_DEVICE_FRIENDLY_NAME)
import smartcard.Exceptions


def main():
    """ Ask the first reader/driver to return its Vendor Name if any """
    card_connection = readers()[0].createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT,
        disposition=SCARD_LEAVE_CARD)

    try:
        name = card_connection.getAttrib(SCARD_ATTR_VENDOR_NAME)
        print ''.join([chr(char) for char in name])

        name = card_connection.getAttrib(SCARD_ATTR_DEVICE_FRIENDLY_NAME)
        print ''.join([chr(char) for char in name])
    except smartcard.Exceptions.SmartcardException, message:
        print "Exception:", message

if __name__ == "__main__":
    main()
