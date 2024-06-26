#! /usr/bin/env python3

#   SCardGetStatusChange_PnP_Events.py: Unit test for PnP events
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


from smartcard.scard import (SCARD_STATE_UNAWARE, SCARD_STATE_IGNORE,
                             SCARD_STATE_CHANGED, SCARD_STATE_UNKNOWN,
                             SCARD_STATE_UNAVAILABLE, SCARD_STATE_EMPTY,
                             SCARD_STATE_PRESENT, SCARD_STATE_ATRMATCH,
                             SCARD_STATE_EXCLUSIVE, SCARD_STATE_INUSE,
                             SCARD_STATE_MUTE, SCARD_STATE_UNPOWERED,
                             SCardEstablishContext,
                             SCardGetStatusChange,
                             SCardListReaders,
                             SCardGetErrorMessage,
                             SCardReleaseContext,
                             SCARD_SCOPE_USER)
from smartcard.util import toHexString
import time

RED = "\033[0;31m"
BLUE = "\033[0;34m"
NORMAL = "\033[00m"


def scardstate2text(cardstate):
    state = list()
    states_dict = {
        SCARD_STATE_UNAWARE: "SCARD_STATE_UNAWARE",
        SCARD_STATE_IGNORE: "SCARD_STATE_IGNORE",
        SCARD_STATE_CHANGED: "SCARD_STATE_CHANGED",
        SCARD_STATE_UNKNOWN: "SCARD_STATE_UNKNOWN",
        SCARD_STATE_UNAVAILABLE: " SCARD_STATE_UNAVAILABLE",
        SCARD_STATE_EMPTY: "SCARD_STATE_EMPTY",
        SCARD_STATE_PRESENT: "SCARD_STATE_PRESENT",
        SCARD_STATE_ATRMATCH: "SCARD_STATE_ATRMATCH",
        SCARD_STATE_EXCLUSIVE: "SCARD_STATE_EXCLUSIVE",
        SCARD_STATE_INUSE: "SCARD_STATE_INUSE",
        SCARD_STATE_MUTE: "SCARD_STATE_MUTE",
        SCARD_STATE_UNPOWERED: "SCARD_STATE_UNPOWERED"}
    for s in states_dict.keys():
        if (cardstate & s):
            state.append(states_dict[s])
    return state


def dumpStates(states):
    for state in states:
        readername, eventstate, atr = state
        print("readername:", readername)
        print("eventstate:", scardstate2text(eventstate))
        print("eventstate hw:", eventstate // (2 ** 16))
        print("atr:", toHexString(atr))
        readerstates[readername] = (readername, eventstate)


hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)

# initialise the states of the readers
readerstates = {}

# List the available readers
hresult, readers = SCardListReaders(hcontext, [])
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)

# Add the PnP special reader
reader = "\\\\?PnP?\\Notification"
readerstates[reader] = (reader, SCARD_STATE_UNAWARE)

print("values:", readerstates.values())

# Get the initial states
hresult, states = SCardGetStatusChange(hcontext, 0,
                                       list(readerstates.values()))
dumpStates(states)

print("Connect a reader within the next 5 seconds")
time.sleep(5)

before = time.time()
# wait 1 second. SCardGetStatusChange() should return immediately
hresult, states = SCardGetStatusChange(hcontext, 1_000,
                                       list(readerstates.values()))
print(SCardGetErrorMessage(hresult))
duration = time.time() - before
dumpStates(states)

print("Duration:", duration)
if duration < 0.5:
    print(BLUE, "===========> OK", NORMAL)
else:
    print(RED, "===========> FAILED", NORMAL)
print()

hresult = SCardReleaseContext(hcontext)
print(SCardGetErrorMessage(hresult))
