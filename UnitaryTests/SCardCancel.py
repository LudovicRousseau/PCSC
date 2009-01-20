#! /usr/bin/env python

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
if hresult!=0:
	raise  'Failed to establish context: ' + SCardGetErrorMessage(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult!=0:
	raise error, 'Failed to list readers: ' + SCardGetErrorMessage(hresult)
print 'PC/SC Readers:', readers

readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 0, readerstates.values())
if hresult!=SCARD_S_SUCCESS:
    raise  'Failed to SCardGetStatusChange: ' + SCardGetErrorMessage(hresult)
print newstates
for state in newstates:
    readername, eventstate, atr = state
    readerstates[readername] = ( readername, eventstate )

t = threading.Thread(target=cancel)
t.start()

hresult, newstates = SCardGetStatusChange(hcontext, 10000, readerstates.values())
if hresult!=SCARD_S_SUCCESS and hresult!=SCARD_E_TIMEOUT:
    raise  'Failed to SCardGetStatusChange: ' + SCardGetErrorMessage(hresult)

hresult = SCardReleaseContext(hcontext)
if hresult!=SCARD_S_SUCCESS:
	raise  'Failed to release context: ' + SCardGetErrorMessage(hresult)

