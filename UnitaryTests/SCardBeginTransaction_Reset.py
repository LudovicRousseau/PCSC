#! /usr/bin/env python3

#   SCardBeginTransaction_ExclusiceMode.py : Unitary test for SCardDisconnect()
#   Copyright (C) 2016  Ludovic Rousseau
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

# Check that a transaction will not reset a card while another
# transaction is ongoing.
# Test for the patch in 74656f24db3da1532040a1775ceffa225fbc3d00

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *

hresult, hcontext1 = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext1, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print('PC/SC Readers:', readers)
reader = readers[0]
print("Using reader:", reader)

hresult, hcontext2 = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, hcard1, dwActiveProtocol = SCardConnect(hcontext1, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult, hcard2, dwActiveProtocol = SCardConnect(hcontext1, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardBeginTransaction(hcard1)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# try to reset the card
hresult = SCardEndTransaction(hcard2, SCARD_UNPOWER_CARD)
if hresult != SCARD_S_SUCCESS:
    # we should get an error here
    # but the card shall not be reset
    if hresult == SCARD_E_SHARING_VIOLATION:
        print("We got the expected error")
    else:
        raise BaseSCardException(hresult)

hresult, reader, state, protocol, atr = SCardStatus(hcard1)
if hresult == SCARD_W_RESET_CARD:
    print("The test failed")
    print("The card should not have been reset")
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardEndTransaction(hcard1, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardDisconnect(hcard1, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardDisconnect(hcard2, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext1)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
