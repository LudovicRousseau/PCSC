#! /usr/bin/env python3

#   SCardConnect_DIRECT.py : Unitary test for SCardConnect in DIRECT mode
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

# MSDN indicates that pdwActiveProtocol must be set to
# SCARD_PROTOCOL_UNDEFINED if SCARD_SHARE_DIRECT is used. This behavior
# has been implemented in revision 4332 but reverted in revision 4940 so
# that the protocol is not negotiated again

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

# the card should be reseted or inserted just before execution

# Connect in SCARD_SHARE_DIRECT mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
        SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

print("dwActiveProtocol:", dwActiveProtocol)

# Reconnect in SCARD_SHARE_DIRECT mode
hresult, dwActiveProtocol = SCardReconnect(hcard,
        SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# ActiveProtocol should be SCARD_PROTOCOL_UNDEFINED (0)
print("dwActiveProtocol:", dwActiveProtocol)
if SCARD_PROTOCOL_UNDEFINED != dwActiveProtocol:
    raise Exception('dwActiveProtocol should be SCARD_PROTOCOL_UNDEFINED')

hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

print("dwActiveProtocol:", dwActiveProtocol)
oldActiveProtocol = dwActiveProtocol

# Reconnect in SCARD_SHARE_DIRECT mode
hresult, dwActiveProtocol = SCardReconnect(hcard,
        SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# ActiveProtocol should be SCARD_PROTOCOL_UNDEFINED (0)
print("dwActiveProtocol:", dwActiveProtocol)
if oldActiveProtocol != dwActiveProtocol:
    raise Exception('dwActiveProtocol should be like before')

hresult = SCardDisconnect(hcard, SCARD_RESET_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
