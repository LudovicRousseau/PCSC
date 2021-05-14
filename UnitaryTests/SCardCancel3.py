#! /usr/bin/env python3

#   SCardCancel3.py : Unitary test for SCardCancel()
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

# Check event after SCardCancel is correctly handled

# When a SCardGetStatusChange() was cancelled then a next PC/SC call
# after the SCardGetStatusChange() may fail with a strange error code if
# the event waited in SCardGetStatusChange() occurs.

# The bug has been fixed in 57b0ba5a200bcbf1c489d39261340324392a8e8a

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
import threading
import time
import sys


def getstatuschange():
    # this call will be cancelled
    hresult, newstates = SCardGetStatusChange(hcontext, 10000,
            list(readerstates.values()))
    print("SCardGetStatusChange()", SCardGetErrorMessage(hresult))
    if hresult != SCARD_S_SUCCESS and hresult != SCARD_E_TIMEOUT:
        if SCARD_E_CANCELLED == hresult:
            print("Cancelled")
        else:
            raise BaseSCardException(hresult)
    print("Finished")

    print("Remove the card and press enter")
    sys.stdin.read(1)

    # try another PC/SC call. It should return SCARD_W_REMOVED_CARD and
    # not something else
    hresult, reader, state, protocol, atr = SCardStatus(hcard)
    if hresult != SCARD_S_SUCCESS:
        if hresult != SCARD_W_REMOVED_CARD:
            raise BaseSCardException(hresult)
        else:
            print("Card removed")

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print('PC/SC Readers:', readers)

readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 0,
        list(readerstates.values()))
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(newstates)
for state in newstates:
    readername, eventstate, atr = state
    readerstates[readername] = (readername, eventstate)

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

t = threading.Thread(target=getstatuschange)
t.start()

time.sleep(1)
print("cancel")
hresult = SCardCancel(hcontext)
if hresult != SCARD_S_SUCCESS:
    print('Failed to SCardCancel: ' + SCardGetErrorMessage(hresult))

time.sleep(5)
hresult = SCardReleaseContext(hcontext)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
