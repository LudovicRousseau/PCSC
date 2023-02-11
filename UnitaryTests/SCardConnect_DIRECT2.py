#! /usr/bin/env python3

#   SCardConnect_DIRECT2.py : Unitary test for SCardReconnect
#   Copyright (C) 2010  Ludovic Rousseau
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

# Scenario
# use a T=1 card, a TPDU reader
# Connect in SCARD_SHARE_SHARED
# driver should negotiate PPS
# Disconnect
# Connect in SCARD_SHARE_DIRECT
# driver should NOT negotiate PPS (the card has not been reset)
# Disconnect
# Connect in SCARD_SHARE_SHARED
# driver should NOT negotiate PPS (the card has not been reset)
# Disconnect

# same issue with Reconnect instead of connect
# bug fixed in revision 4940

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *

SELECT = [0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00]

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print('PC/SC Readers:', readers)

reader = readers[0]
print("Using reader:", reader)

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Transmit
hresult, response = SCardTransmit(hcard, dwActiveProtocol, SELECT)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(response)

# Disconnect
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Connect in SCARD_SHARE_DIRECT mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Disconnect
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Transmit
hresult, response = SCardTransmit(hcard, dwActiveProtocol, SELECT)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(response)

# Reconnect in SCARD_SHARE_DIRECT mode
hresult, dwActiveProtocol = SCardReconnect(hcard,
        SCARD_SHARE_DIRECT, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Disconnect
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# Transmit
hresult, response = SCardTransmit(hcard, dwActiveProtocol, SELECT)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(response)

# Disconnect
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
