#! /usr/bin/env python3

"""
transmit_card_removed.py
Copyright (C) 2020  Ludovic Rousseau
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

from time import time

"""
SCardTransmit() should return SCARD_E_NO_SMARTCARD when a card is
removed during an APDU exchange.

SCardStatus() should report that the card is no more present.
"""

from smartcard.scard import *
from smartcard.util import toBytes, toHexString
from smartcard.pcsc.PCSCExceptions import *

RED = "\033[0;31m"
BLUE = "\033[0;34m"
NORMAL = "\033[00m"

# define the APDUs used in this script
SELECT = toBytes("00 A4 04 00 06 A0 00 00 00 18 FF")
TIME = toBytes("80 38 00 10")


def pcsc_error(command, hresult):
    h = hresult & 0xFFFFFFFF
    return "{}: {} ({})".format(command, SCardGetErrorMessage(hresult), hex(h))


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

# Transmit
hresult, response = SCardTransmit(hcard, dwActiveProtocol, SELECT)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(toHexString(response))

print("Remove the card")
# Transmit
hresult, response = SCardTransmit(hcard, dwActiveProtocol, TIME)
print(pcsc_error("transmit", hresult))
if hresult != SCARD_E_NO_SMARTCARD:
    print(RED + "Wrong result" + NORMAL)
else:
    print(BLUE + "Correct result" + NORMAL)
if hresult == SCARD_S_SUCCESS:
    print(toHexString(response))

# status
hresult, reader, state, protocol, atr = SCardStatus(hcard)
print(pcsc_error("status", hresult))
print("state:", hex(state))
print("Absent:", (state & 2) != 0)
print("Present:", (state & 4) != 0)
print("ATR:", toHexString(atr))
before = time()
while hresult == SCARD_S_SUCCESS:
    print(".", end="")
    hresult, reader, state, protocol, atr = SCardStatus(hcard)

print()
print(pcsc_error("status", hresult))

after = time()
delta = after - before
print("Detected after: {} ms".format(delta * 1000))

# Disconnect
hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
