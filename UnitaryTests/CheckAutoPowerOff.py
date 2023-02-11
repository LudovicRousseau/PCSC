#! /usr/bin/env python3
# -*- coding: UTF-8 -*-

#   CheckAutoPowerOff.py : Unit test for card auto power off
#   Copyright (C) 2018  Ludovic Rousseau
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

# check the card is not auto powered off after a SCardReconnect()

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
import sys
import time


RED = "\033[0;31m"
BLUE = "\033[0;34m"
NORMAL = "\033[00m"

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

count = -1
print("Remove the card")
while True:
    hresult, reader, state, protocol, atr = SCardStatus(hcard)
    if hresult == SCARD_W_REMOVED_CARD:
        print("\nInsert the card")
        while hresult != SCARD_S_SUCCESS:
            hresult, dwActiveProtocol = SCardReconnect(hcard,
                SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
            print(".", end="")
            sys.stdout.flush()
            time.sleep(1)
        print("\nWaiting for (bogus) card auto power off")
        count = 0
        hresult, reader, state, protocol, atr = SCardStatus(hcard)

    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)
    #print(SCardGetErrorMessage(hresult))

    print(".", end="")
    sys.stdout.flush()

    if protocol == 0:
        print(RED + "\nError: the card is powered off" + NORMAL)
        break

    if count >= 0:
        count += 1
        if (count > 10):
            print(BLUE + "\nTest passed" + NORMAL)
            break
    time.sleep(1)

hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
