#! /usr/bin/env python3

#   SCardGetStatusChange.py : Unit tests for unlock a SCardGetStatusChange()
#   Copyright (C) 2008-2009  Ludovic Rousseau
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

"""
test SCardGetStatusChange
"""

from smartcard.scard import (
    SCARD_SCOPE_USER,
    SCARD_STATE_UNAWARE,
    SCardEstablishContext,
    SCardGetErrorMessage,
    SCardGetStatusChange,
    SCardListReaders,
    SCardReleaseContext,
)

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

# wait for a change with a 10s timeout
(hresult, states) = SCardGetStatusChange(hcontext, 10000, list(readerstates.values()))
print(SCardGetErrorMessage(hresult))
print(states)

for state in states:
    readername, eventstate, atr = state
    print("readername:", readername)
    print("eventstate:", hex(eventstate))
    print("atr:", atr)
    readerstates[readername] = (readername, eventstate)
print("values", readerstates.values())

hresult = SCardReleaseContext(hcontext)
print(SCardGetErrorMessage(hresult))
