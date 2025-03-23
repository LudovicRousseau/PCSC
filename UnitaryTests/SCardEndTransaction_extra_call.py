#! /usr/bin/env python3

#   SCardEndTransaction_Disconnect.py : Unitary test for SCardEndTransaction()
#   Copyright (C) 2013  Ludovic Rousseau
#
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

# A bug has been corrected in revision 6748
# SCardEndTransaction() should return SCARD_E_NOT_TRANSACTED if called
# more times than SCardBeginTransaction()

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print("PC/SC Readers:", readers)
reader = readers[0]
print("Using reader:", reader)

hresult, hcard, dwActiveProtocol = SCardConnect(
    hcontext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY
)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardBeginTransaction(hcard)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print("SCardBeginTransaction()", SCardGetErrorMessage(hresult))

hresult = SCardEndTransaction(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print("first SCardEndTransaction()", SCardGetErrorMessage(hresult))

hresult = SCardEndTransaction(hcard, SCARD_LEAVE_CARD)
print("second SCardEndTransaction()", SCardGetErrorMessage(hresult))
if hresult == SCARD_E_NOT_TRANSACTED:
    print("OK")
else:
    print("ERROR! SCardEndTransaction() should have failed")

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
