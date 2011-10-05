#! /usr/bin/env python

"""
#   SCardGetAttrib.py: get SCARD_ATTR_VENDOR_IFD_SERIAL_NO PC/SC attribute
#   Copyright (C) 2010  Ludovic Rousseau
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

# SCARD_ATTR_VENDOR_IFD_SERIAL_NO support has been added in revision 4956

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
import sys

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print 'PC/SC Readers:', readers

for reader in readers:
    hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
        SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    hresult, attrib = SCardGetAttrib(hcard, SCARD_ATTR_VENDOR_IFD_SERIAL_NO)
    print reader, ":",
    print "".join([chr(x) for x in attrib])
    print SCardGetErrorMessage(hresult)

    hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
