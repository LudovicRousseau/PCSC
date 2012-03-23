#! /usr/bin/env python

"""
#   control_get_firmware.py: get firmware version of Gemalto readers
#   Copyright (C) 2009-2012  Ludovic Rousseau
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
from smartcard.pcsc.PCSCPart10 import (SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD, SCARD_CTL_CODE, getTlvProperties)

for reader in readers():
    cardConnection = reader.createConnection()
    cardConnection.connect(mode=SCARD_SHARE_DIRECT,
        disposition=SCARD_LEAVE_CARD)

    print "Reader:", reader

    # properties returned by IOCTL_FEATURE_GET_TLV_PROPERTIES
    properties = getTlvProperties(cardConnection)

    # Gemalto devices supports a control code to get firmware
    if properties['PCSCv2_PART10_PROPERTY_wIdVendor'] == 0x08E6:
        get_firmware = [0x02]
        IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE = SCARD_CTL_CODE(1)
        res = cardConnection.control(IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE,
            get_firmware)
        print " Firmware:", "".join([chr(x) for x in res])
    else:
        print " Not a Gemalto reader"
        try:
            res = properties['PCSCv2_PART10_PROPERTY_sFirmwareID']
            print " Firmware:", frimware
        except KeyError:
            print " PCSCv2_PART10_PROPERTY_sFirmwareID not supported"
