#! /usr/bin/env python3

#   SCardBeginTransaction_deadlock.py: Unitary test for locking in
#   SCardBeginTransaction, SCardTransmit, SCardStatus and SCardReconnect
#   Copyright (C) 2012  Ludovic Rousseau
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, see <http://www.gnu.org/licenses/>.

# [Muscle] [PATCH] fix deadlock in PCSC-Lite
# http://archives.neohapsis.com/archives/dev/muscle/2012-q2/0109.html
# fixed in revisions 6358, 6359, 6360 and 6361

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
import threading
import time

def myThread(reader):
    print("thread 2: SCardConnect")
    hresult, hcard2, dwActiveProtocol = SCardConnect(hcontext1, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    # wait for the 1st thread to begin a transaction
    time.sleep(1)

    """
    # check for SCardBeginTransaction
    print("thread 2: SCardBeginTransaction")
    hresult = SCardBeginTransaction(hcard2)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    print("thread 2: SCardEndTransaction")
    hresult = SCardEndTransaction(hcard2, SCARD_LEAVE_CARD)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)
    """

    """
    # check for SCardTransmit()
    SELECT = [0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00]
    print("thread 2: SCardTransmit")
    hresult, response = SCardTransmit(hcard2, dwActiveProtocol, SELECT)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)
    print(response)
    """

    """
    # check for SCardStatus()
    print("thread 2: SCardStatus")
    hresult, reader, state, protocol, atr = SCardStatus(hcard2)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)
    """

    # check for SCardReconnect()
    print("thread 2: SCardReconnect")
    hresult, dwActiveProtocol = SCardReconnect(hcard2,
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    print("thread 2: SCardDisconnect")
    hresult = SCardDisconnect(hcard2, SCARD_LEAVE_CARD)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

print("thread 1: SCardEstablishContext")
hresult, hcontext1 = SCardEstablishContext(SCARD_SCOPE_USER)
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

print("thread 1: SCardListReaders")
hresult, readers = SCardListReaders(hcontext1, [])
if hresult != SCARD_S_SUCCESS:
    raise ListReadersException(hresult)
print('PC/SC Readers:', readers)
reader = readers[0]
print("Using reader:", reader)

# second thread
t = threading.Thread(target=myThread, args=(reader, ))
t.start()

# wait for the 1st thread to begin a transaction
time.sleep(0.5)

print("thread 1: SCardConnect")
hresult, hcard1, dwActiveProtocol = SCardConnect(hcontext1, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

print("thread 1: SCardBeginTransaction")
hresult = SCardBeginTransaction(hcard1)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

time.sleep(2)

print("thread 1: SCardEndTransaction")
hresult = SCardEndTransaction(hcard1, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

# give time to thread2 to finish
time.sleep(1)

print("thread 1: SCardDisconnect")
hresult = SCardDisconnect(hcard1, SCARD_LEAVE_CARD)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)

print("thread 1: SCardReleaseContext")
hresult = SCardReleaseContext(hcontext1)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
