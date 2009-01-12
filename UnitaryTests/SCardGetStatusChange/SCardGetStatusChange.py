#! /usr/bin/env python
# -*- coding: UTF-8 -*-

"""Unit tests for unlock a SCardGetStatusChange()

Copyright 2008 Ludovic Rousseau

"""


from smartcard.System import readers
from smartcard.scard import *

import sys

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
    print "eventstate:", hex(eventstate)
    print "atr:", atr
    readerstates[readername] = ( readername, eventstate )
print "values", readerstates.values()

# wait for a change with a 10s timeout
(hresult, states) = SCardGetStatusChange(hcontext, 10000, readerstates.values())
print SCardGetErrorMessage(hresult)
print states

for state in states:
    readername, eventstate, atr = state
    print "readername:", readername
    print "eventstate:", hex(eventstate)
    print "atr:", atr
    readerstates[readername] = ( readername, eventstate )
print "values", readerstates.values()

hresult = SCardReleaseContext(hcontext)

