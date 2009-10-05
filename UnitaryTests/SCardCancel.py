#! /usr/bin/env python

#   SCardCancel.py : Unitary test for SCardCancel()
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


from smartcard.scard import *
import threading
import time

def cancel():
    time.sleep(1)
    print "cancel"
    hresult = SCardCancel(hcontext)
    if hresult!=0:
        print 'Failed to SCardCancel: ' + SCardGetErrorMessage(hresult)

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult!=SCARD_S_SUCCESS:
	raise Exception('Failed to establish context: ' + SCardGetErrorMessage(hresult))

hresult, readers = SCardListReaders(hcontext, [])
if hresult!=SCARD_S_SUCCESS:
	raise Exception('Failed to list readers: ' + SCardGetErrorMessage(hresult))
print 'PC/SC Readers:', readers

readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 0, readerstates.values())
if hresult!=SCARD_S_SUCCESS:
    raise Exception('Failed to SCardGetStatusChange: ' + SCardGetErrorMessage(hresult))
print newstates
for state in newstates:
    readername, eventstate, atr = state
    readerstates[readername] = ( readername, eventstate )

t = threading.Thread(target=cancel)
t.start()

hresult, newstates = SCardGetStatusChange(hcontext, 10000, readerstates.values())
print "SCardGetStatusChange()", SCardGetErrorMessage(hresult)
if hresult!=SCARD_S_SUCCESS and hresult!=SCARD_E_TIMEOUT:
    if SCARD_E_CANCELLED == hresult:
        pass
    else:
        raise Exception('Failed to SCardGetStatusChange: ' + SCardGetErrorMessage(hresult))

hresult = SCardReleaseContext(hcontext)
print "SCardReleaseContext()", SCardGetErrorMessage(hresult)
if hresult!=SCARD_S_SUCCESS:
	raise Exception('Failed to release context: ' + SCardGetErrorMessage(hresult))

