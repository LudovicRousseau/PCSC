#! /usr/bin/env python
# -*- coding: UTF-8 -*-

"""Unit tests for unlock a SCardGetStatusChange()

Copyright 2009 Ludovic Rousseau

"""


from smartcard.System import readers
from smartcard.scard import *
from smartcard.util import toHexString

import sys

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
        SCARD_STATE_UNPOWERED: "SCARD_STATE_UNPOWERED"
        }
    for s in states_dict.keys():
        if (cardstate & s):
            state.append(states_dict[s])
    return state

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
hresult, readers = SCardListReaders(hcontext, [])

# initialise the states of the readers
readerstates = {}
for reader in readers:
    readerstates[reader] = ( reader, SCARD_STATE_UNAWARE )
print "values", readerstates.values()
(hresult, states) = SCardGetStatusChange(hcontext, 0, readerstates.values())
print SCardGetErrorMessage(hresult)
print states

for state in states:
    readername, eventstate, atr = state
    print "readername:", readername
    print "eventstate:", scardstate2text(eventstate)
    print "atr:", toHexString(atr)
    readerstates[readername] = ( readername, eventstate )
print "values", readerstates.values()

# wait for a change with a 10s timeout
reader = "\\\\?PnP?\\Notification"
readerstates[reader] = ( reader, SCARD_STATE_UNAWARE )

(hresult, states) = SCardGetStatusChange(hcontext, 10000, readerstates.values())
print SCardGetErrorMessage(hresult)
print states

for state in states:
    readername, eventstate, atr = state
    print "readername:", readername
    print "eventstate:", scardstate2text(eventstate)
    print "atr:", toHexString(atr)
    readerstates[readername] = ( readername, eventstate )
print "values", readerstates.values()

hresult = SCardReleaseContext(hcontext)

