#! /usr/bin/env python3

#   SCardGetStatusChange_nonBlock.py : Unit tests for SCardGetStatusChange()
#   Copyright (C) 2024  Ludovic Rousseau
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

# check we can use SCardListReaders() while SCardGetStatusChange() is
# running

import threading
import time
from smartcard.scard import (SCardEstablishContext, SCardListReaders,
                             SCardGetStatusChange, SCardGetErrorMessage,
                             SCardReleaseContext, SCARD_SCOPE_USER, SCARD_STATE_UNAWARE)


def non_blocking():
    print("In the thread")
    hresult, hcontext2 = SCardEstablishContext(SCARD_SCOPE_USER)
    print(SCardGetErrorMessage(hresult))
    for _ in range(10):
        print("SCardListReaders: ", end="")
        hresult, _ = SCardListReaders(hcontext2, [])
        print(SCardGetErrorMessage(hresult))
        time.sleep(1)

    hresult = SCardReleaseContext(hcontext2)
    print(SCardGetErrorMessage(hresult))

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

before = time.time()

thread = threading.Thread(target=non_blocking)
thread.start()

# wait for a change with a 10s timeout
print("wait")
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

thread.join()

after = time.time()
delta = after - before
print(f"delta time: {delta} seconds")
if delta < 11:
    print("OK")
else:
    print("************")
    print("test FAILURE")
    print("************")

hresult = SCardReleaseContext(hcontext)
print(SCardGetErrorMessage(hresult))
