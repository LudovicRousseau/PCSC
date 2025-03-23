#! /usr/bin/env python3

"""
#   SCardGetAttrib.py: get SCARD_ATTR_VENDOR_IFD_SERIAL_NO PC/SC attribute
#   Copyright (C) 2010,2016  Ludovic Rousseau
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

# SCARD_ATTR_VENDOR_IFD_SERIAL_NO support has been added in ccid 1.3.13
# SCARD_ATTR_ATR_STRING support has been added in ccid 0.9.0

from struct import unpack
from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
from smartcard.util import toHexString, toASCIIString

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print("PC/SC Readers:", readers)

for reader in readers:
    hresult, hcard, dwActiveProtocol = SCardConnect(
        hcontext, reader, SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY
    )
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    print("reader:", reader)
    for attribute in (
        SCARD_ATTR_VENDOR_IFD_SERIAL_NO,
        SCARD_ATTR_ATR_STRING,
        SCARD_ATTR_CHANNEL_ID,
    ):
        hresult, attrib = SCardGetAttrib(hcard, attribute)
        print(hex(attribute), end=" ")
        if hresult != SCARD_S_SUCCESS:
            print(SCardGetErrorMessage(hresult))
        else:
            print(attrib, toHexString(attrib), toASCIIString(attrib))
            if attribute is SCARD_ATTR_CHANNEL_ID:
                # get the DWORD value
                DDDDCCCC = unpack("i", bytearray(attrib))[0]
                DDDD = DDDDCCCC >> 16
                if DDDD == 0x0020:
                    bus = (DDDDCCCC & 0xFF00) >> 8
                    addr = DDDDCCCC & 0xFF
                    print(f" USB: bus: {bus}, addr: {addr}")

    hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
