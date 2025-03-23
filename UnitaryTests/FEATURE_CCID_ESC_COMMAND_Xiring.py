#! /usr/bin/env python3

"""
#   FEATURE_CCID_ESC_COMMAND_Xiring.py: Unitary test for
#   FEATURE_CCID_ESC_COMMAND
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
from smartcard.pcsc.PCSCPart10 import (
    getFeatureRequest,
    hasFeature,
    getTlvProperties,
    FEATURE_CCID_ESC_COMMAND,
    SCARD_SHARE_DIRECT,
)

# use the first reader
card_connection = readers()[0].createConnection()
card_connection.connect(mode=SCARD_SHARE_DIRECT)

# get CCID Escape control code
feature_list = getFeatureRequest(card_connection)

ccid_esc_command = hasFeature(feature_list, FEATURE_CCID_ESC_COMMAND)
if ccid_esc_command is None:
    raise Exception("The reader does not support FEATURE_CCID_ESC_COMMAND")

# get the TLV PROPERTIES
tlv = getTlvProperties(card_connection)

# check we are using a Xiring Leo v1 or v2 reader
if tlv["PCSCv2_PART10_PROPERTY_wIdVendor"] == 0x0F14 and (
    tlv["PCSCv2_PART10_PROPERTY_wIdProduct"] in [0x0037, 0x0038]
):

    # proprietary escape command for Xiring Leo readers
    version = [ord(c) for c in "VERSION"]
    res = card_connection.control(ccid_esc_command, version)
    print(res)
    print("VERSION:", "".join([chr(x) for x in res]))

    serial = [ord(c) for c in "GET_SN"]
    res = card_connection.control(ccid_esc_command, serial)
    print(res)
    print("GET_SN:", "".join([chr(x) for x in res]))
else:
    print("Xiring Leo reader not found")
