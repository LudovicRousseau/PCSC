#! /usr/bin/env python3

#   SCardGetStatusChange.py : Unitary test for SCardGetStatusChange()
#   Copyright (C) 2011  Ludovic Rousseau
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

# Check the return value of SCardGetStatusChange() after a reader has
# been removed.
#
# Windows has a different behavior (Viktor Tarasov) :
# https://github.com/viktorTarasov/OpenSC/commit/62bda63bd66c4849c0ca4303a9682fb6f6bacd7d
# /* When token is hot-unplugged:
#  * - in Linux (pcsc-lite)
#  * -- SCardGetStatusChange returns OK;
#  * -- current reader state is 'UNKNOWN';
#  * -- 'Refresh-attributes' returns 'SC_ERROR_READER_DETACHED'.
#  *
#  * - in Windows (WinSCard):
#  * -- SCardGetStatusChange fails with SCARD_E_NO_READERS_AVAILABLE;
#  * -- 'Refresh-attributes' returns 'SC_ERROR_NO_READERS_FOUND'.
#  *
#  * - FIXME: Mac?
#  */

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *


def parseEventState(eventstate):
    stateList = list()
    states = {
        SCARD_STATE_IGNORE: "Ignore",
        SCARD_STATE_CHANGED: "Changed",
        SCARD_STATE_UNKNOWN: "Unknown",
        SCARD_STATE_UNAVAILABLE: "Unavailable",
        SCARD_STATE_EMPTY: "Empty",
        SCARD_STATE_PRESENT: "Present",
        SCARD_STATE_ATRMATCH: "ATR match",
        SCARD_STATE_EXCLUSIVE: "Exclusive",
        SCARD_STATE_INUSE: "In use",
        SCARD_STATE_MUTE: "Mute",
        SCARD_STATE_UNPOWERED: "Unpowered",
    }
    for state in states:
        if eventstate & state:
            stateList.append(states[state])
    return stateList


hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print("PC/SC Readers:", readers)

readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 0, list(readerstates.values()))
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(newstates)
for readerState in newstates:
    readername, eventstate, atr = readerState
    readerstates[readername] = (readername, eventstate)

print("Remove the reader")

hresult, newstates = SCardGetStatusChange(hcontext, 10000, list(readerstates.values()))
print("SCardGetStatusChange()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS and hresult != SCARD_E_TIMEOUT:
    raise BaseSCardException(hresult)
for readerState in newstates:
    readername, eventstate, atr = readerState
    print(readername, hex(eventstate))
    print(parseEventState(eventstate))

hresult = SCardReleaseContext(hcontext)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
