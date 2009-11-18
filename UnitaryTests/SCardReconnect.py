#! /usr/bin/env python

#   SCardReconnect.py : Unitary test for SCardReconnect
#   Copyright (C) 2009  Ludovic Rousseau
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

# SCardReconnect() should block instead of returning SCARD_E_SHARING_VIOLATION

from smartcard.scard import *
import sys

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to establish context: ' + SCardGetErrorMessage(hresult))

hresult, readers = SCardListReaders(hcontext, [])
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to list readers: ' + SCardGetErrorMessage(hresult))
print 'PC/SC Readers:', readers

print "Using reader:", readers[0]

# Connect in SCARD_SHARE_SHARED mode
hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, readers[0],
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to SCardConnect: ' + SCardGetErrorMessage(hresult))

print "Press enter"
sys.stdin.read(1)

# Reconnect in SCARD_SHARE_DIRECT mode
hresult, dwActiveProtocol = SCardReconnect(hcard,
    SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to SCardReconnect: ' +
        SCardGetErrorMessage(hresult))

hresult = SCardDisconnect(hcard, SCARD_RESET_CARD)
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to SCardDisconnect: ' + SCardGetErrorMessage(hresult))

hresult = SCardReleaseContext(hcontext)
if hresult != SCARD_S_SUCCESS:
    raise Exception('Failed to release context: ' + SCardGetErrorMessage(hresult))
