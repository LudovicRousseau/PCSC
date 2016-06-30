#! /usr/bin/env python

"""
#   FEATURE_GET_TLV_PROPERTIES.py: Unitary test for
#   FEATURE_GET_TLV_PROPERTIES
#   Copyright (C) 2012  Ludovic Rousseau

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

# use the first reader
card_connection = readers()[0].createConnection()
card_connection.connect(mode=SCARD_SHARE_DIRECT)

# get the TLV PROPERTIES
tlv = getTlvProperties(card_connection)

for key in sorted(tlv):
    if key in ["PCSCv2_PART10_PROPERTY_wIdProduct",
            "PCSCv2_PART10_PROPERTY_wIdVendor"]:
        print "%s: 0x%04X" % (key, tlv[key])
    else:
        print "%s: %s" % (key, tlv[key])
