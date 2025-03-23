#! /usr/bin/env python3

#   SCardReconnect.py : Unitary test for SCardReconnect
#   Copyright (C) 2009-2011  Ludovic Rousseau
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

# SCardReconnect() should block instead of returning SCARD_E_SHARING_VIOLATION
# when the reconnection requests exclusive access and the reader is
# already shared.

import sys
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

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(
    hcontext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY
)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# start here another application using the reader in SCARD_SHARE_SHARED
# mode

print("Press enter")
sys.stdin.read(1)

# Reconnect in SCARD_SHARE_EXCLUSIVE mode
hresult, dwActiveProtocol = SCardReconnect(
    hcard, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD
)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardDisconnect(hcard, SCARD_RESET_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
