#! /usr/bin/env python

from smartcard.scard import *
import threading

def cancel():
    print "cancel"
    #SCardCancel(hcontext)

hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult!=0:
	raise  'Failed to establish context: ' + SCardGetErrorMessage(hresult)

hresult, readers = SCardListReaders(hcontext, [])
if hresult!=0:
	raise error, 'Failed to list readers: ' + SCardGetErrorMessage(hresult)
print 'PC/SC Readers:', readers

hresult, hcard, proto = SCardConnect(hcontext, "toto", SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0)
print hresult, SCardGetErrorMessage(hresult)

readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 0, readerstates.values())
if hresult!=SCARD_S_SUCCESS:
	raise  'Failed to : SCardGetStatusChange' + SCardGetErrorMessage(hresult)
print newstates
for state in newstates:
    readername, eventstate, atr = state
    readerstates[readername] = ( readername, eventstate )

hresult, newstates = SCardGetStatusChange(hcontext, 100, readerstates.values())
print hresult, SCARD_E_TIMEOUT
if hresult!=SCARD_S_SUCCESS and hresult!=SCARD_E_TIMEOUT:
	raise  'Failed to : SCardGetStatusChange' + SCardGetErrorMessage(hresult)

t = threading.Thread(target=cancel)
t.start()


hresult = SCardReleaseContext(hcontext)
if hresult!=SCARD_S_SUCCESS:
	raise  'Failed to release context: ' + SCardGetErrorMessage(hresult)

