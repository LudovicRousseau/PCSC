#! /usr/bin/env python3

"""
#   MCT_ReaderDirect.py : Unitary test for Multifunctional Card Terminal
#   Copyright (C) 2009-2010  Ludovic Rousseau
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
from smartcard.pcsc.PCSCPart10 import (
    SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD,
    FEATURE_MCT_READER_DIRECT,
    getFeatureRequest,
    hasFeature,
)
from smartcard.util import toHexString


def parse_info(bytes):
    """parse the SECODER INFO answer"""
    print("parse the SECODER INFO answer:", toHexString(bytes))

    sw = bytes[-2:]
    del bytes[-2:]

    while len(bytes):
        tag = bytes[0]
        length = bytes[1]
        data = bytes[2 : 2 + length]

        print("tag: %02X, length: %2d:" % (tag, length), end=" ")
        if tag in [0x40, 0x80, 0x81, 0x83, 0x84]:
            print("'%s'" % "".join([chr(x) for x in data]))
        else:
            print(toHexString(data))

        del bytes[: 2 + length]
    print("SW:", toHexString(sw))


def parse_select(bytes):
    """parse the SECODER SELECT APPLICATION answer"""
    print("parse the SECODER SELECT APPLICATION answer:", toHexString(bytes))

    print("Activation ID:", toHexString(bytes[0:4]))
    print("Interface Version: '%s'" % "".join([chr(x) for x in bytes[5:11]]))
    print("Language Code:", toHexString(bytes[11:15]))
    print("CSI:", toHexString(bytes[15:18]))
    print("Application Identifier:", toHexString(bytes[18:23]))
    print("SW:", toHexString(bytes[23:25]))


def main():
    """main"""
    card_connection = readers()[0].createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

    feature_list = getFeatureRequest(card_connection)
    # print(getPinProperties(card_connection))

    mct_reader_direct = hasFeature(feature_list, FEATURE_MCT_READER_DIRECT)
    if mct_reader_direct is None:
        raise Exception("The reader does not support MCT_READER_DIRECT")

    secoder_info = [0x20, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00]
    res = card_connection.control(mct_reader_direct, secoder_info)
    parse_info(res)

    secoder_select = [
        0x20,
        0x71,
        0x00,
        0x00,
        0x00,
        0x00,
        0x14,
        0x00,
        0x80,
        0x05,
        0x31,
        0x2E,
        0x31,
        0x2E,
        0x30,
        0x84,
        0x02,
        0x64,
        0x65,
        0x90,
        0x01,
        0x01,
        0x85,
        0x03,
        ord("g"),
        ord("k"),
        ord("p"),
        0x00,
        0x00,
    ]
    res = card_connection.control(mct_reader_direct, secoder_select)
    parse_select(res)


if __name__ == "__main__":
    main()
