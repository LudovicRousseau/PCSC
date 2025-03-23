#! /usr/bin/env python3

"""
#   FEATURE_GET_TLV_PROPERTIES.py: Unitary test for
#   FEATURE_GET_TLV_PROPERTIES
#   Copyright (C) 2012,2016  Ludovic Rousseau

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

# You have to enable the use of Escape commands with the
# DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED bit in the ifdDriverOptions
# option of the CCID driver Info.plist file

from smartcard.System import readers
from smartcard.pcsc.PCSCPart10 import getTlvProperties, SCARD_SHARE_DIRECT

# for each reader
for reader in readers():
    print()
    print("Reader:", reader)

    card_connection = reader.createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT)

    # get the TLV PROPERTIES
    tlv = getTlvProperties(card_connection)

    for key in sorted(tlv):
        if key in [
            "PCSCv2_PART10_PROPERTY_wIdProduct",
            "PCSCv2_PART10_PROPERTY_wIdVendor",
        ]:
            print("%s: 0x%04X" % (key, tlv[key]))
        else:
            print("%s: %s" % (key, tlv[key]))
