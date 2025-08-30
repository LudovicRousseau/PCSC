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

from smartcard.pcsc.PCSCPart10 import (
    FEATURE_MCT_READER_DIRECT,
    SCARD_LEAVE_CARD,
    SCARD_SHARE_DIRECT,
    getFeatureRequest,
    hasFeature,
)
from smartcard.System import readers
from smartcard.util import toHexString


def parse_info(data_bytes):
    """parse the SECODER INFO answer"""
    print("parse the SECODER INFO answer:", toHexString(data_bytes))

    sw = data_bytes[-2:]
    del data_bytes[-2:]

    while len(data_bytes):
        tag = data_bytes[0]
        length = data_bytes[1]
        data = data_bytes[2 : 2 + length]

        print(f"tag: {tag:02X}, length: {length:2}:", end=" ")
        if tag in [0x40, 0x80, 0x81, 0x83, 0x84]:
            print("'%s'" % "".join([chr(x) for x in data]))
        else:
            print(toHexString(data))

        del data_bytes[: 2 + length]
    print("SW:", toHexString(sw))


def parse_select(data_bytes):
    """parse the SECODER SELECT APPLICATION answer"""
    print("parse the SECODER SELECT APPLICATION answer:", toHexString(data_bytes))

    print("Activation ID:", toHexString(data_bytes[0:4]))
    print("Interface Version: '%s'" % "".join([chr(x) for x in data_bytes[5:11]]))
    print("Language Code:", toHexString(data_bytes[11:15]))
    print("CSI:", toHexString(data_bytes[15:18]))
    print("Application Identifier:", toHexString(data_bytes[18:23]))
    print("SW:", toHexString(data_bytes[23:25]))


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
