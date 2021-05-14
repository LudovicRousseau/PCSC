#! /usr/bin/env python3

#   SCardBeginTransaction_Disconnect.py : Unitary test for SCardDisconnect()
#   Copyright (C) 2009  Ludovic Rousseau
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

# A bug has been corrected in revision 3467
# The problem occurs if SCardBeginTransaction() are made without
# corresponding SCardEndTransaction(). OpenSC "pkcs11-tool -I" exhibits
# such a behavior.

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print('PC/SC Readers:', readers)
reader = readers[0]
print("Using reader:", reader)

hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardBeginTransaction(hcard)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardBeginTransaction(hcard)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardDisconnect(hcard, SCARD_RESET_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
