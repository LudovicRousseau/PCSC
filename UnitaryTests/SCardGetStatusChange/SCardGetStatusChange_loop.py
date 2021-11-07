#! /usr/bin/env python3
# -*- coding: UTF-8 -*-

#   SCardGetStatusChange.py : Unit tests for unlock a SCardGetStatusChange()
#   Copyright (C) 2010  Ludovic Rousseau
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


from smartcard.System import readers
from smartcard.scard import *


hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
hresult, readers = SCardListReaders(hcontext, [])

# initialise the states of the readers
readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
print("values", readerstates.values())
(hresult, states) = SCardGetStatusChange(hcontext, 0, list(readerstates.values()))
print(SCardGetErrorMessage(hresult))
print(states)

for state in states:
    readername, eventstate, atr = state
    print("readername:", readername)
    print("eventstate:", hex(eventstate))
    print("atr:", atr)
    readerstates[readername] = (readername, eventstate)
print("values", readerstates.values())

try:
    while True:
        # timeout is 1000 ms
        (hresult, states) = SCardGetStatusChange(hcontext, 1000, list(readerstates.values()))
        if hresult != SCARD_S_SUCCESS and hresult != SCARD_E_TIMEOUT:
            raise Exception("SCardGetStatusChange failed: " + SCardGetErrorMessage(hresult))
        print(SCardGetErrorMessage(hresult))
        print(states)

        for state in states:
            print(state)
            readername, eventstate, atr = state
            print("readername:", readername)
            print("eventstate:", hex(eventstate))
            print("atr:", atr)
            readerstates[readername] = (readername, eventstate)
        print("values", readerstates.values())
except KeyboardInterrupt:
    hresult = SCardReleaseContext(hcontext)
    print(SCardGetErrorMessage(hresult))
