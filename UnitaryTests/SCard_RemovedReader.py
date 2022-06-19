#! /usr/bin/env python3

#   SCard_RemovedReader.py.py : Unitary test for SCardReleaseContext
#   Copyright (C) 2022  Ludovic Rousseau
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

# SCardDisconnect() and SCardReleaseContext() should not fail after a
# reader removal. That is what we have on Windows 10.

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *


hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)

reader = readers[0]
print("Using reader:", reader)

hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
print("SCardConnect()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult=hresult)

FAKE_HANDLE = 123456

# should fail
hresult = SCardDisconnect(FAKE_HANDLE, SCARD_LEAVE_CARD)
print("SCardDisconnect()", SCardGetErrorMessage(hresult))
if hresult != SCARD_E_INVALID_HANDLE:
    raise BaseSCardException(hresult=hresult)

# should fail
hresult = SCardReleaseContext(FAKE_HANDLE)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_E_INVALID_HANDLE:
    raise ReleaseContextException(hresult)

print("Remove reader and press Enter")
input()

hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
print("SCardDisconnect()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult=hresult)

# should fail
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
print("SCardDisconnect()", SCardGetErrorMessage(hresult))
if hresult != SCARD_E_INVALID_HANDLE:
    raise BaseSCardException(hresult=hresult)

hresult = SCardReleaseContext(hcontext)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)

# should fail
hresult = SCardReleaseContext(hcontext)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_E_INVALID_HANDLE:
    raise ReleaseContextException(hresult)
