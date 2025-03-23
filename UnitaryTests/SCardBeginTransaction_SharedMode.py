#! /usr/bin/env python3

#   SCardBeginTransaction_SharedMode.py : Unitary test for
#   SCardReleaseContext()
#   Copyright (C) 2018  Ludovic Rousseau
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

# SCardReleaseContext() should not release a PC/SC transaction not
# started by the released context

import threading
import time
from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *

RED = "\033[0;31m"
BLUE = "\033[0;34m"
NORMAL = "\033[00m"


def init_client():
    print("SCardEstablishContext")
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    print("SCardListReaders")
    hresult, readers = SCardListReaders(hcontext, [])
    if hresult != SCARD_S_SUCCESS:
        raise ListReadersException(hresult)
    reader = readers[0]
    print("Using reader:", reader)

    print("SCardConnect")
    hresult, hcard, dwActiveProtocol = SCardConnect(
        hcontext, reader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY
    )
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    return hcontext, hcard, reader, dwActiveProtocol


def thread_transaction():
    # 2nd client
    hcontext2, hcard2, reader, dwActiveProtocol = init_client()

    time.sleep(0.5)

    print("SCardBeginTransaction hcard2")
    hresult = SCardBeginTransaction(hcard2)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    SELECT = [0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00]
    print("SCardTransmit hcard2")
    hresult, response = SCardTransmit(hcard2, dwActiveProtocol, SELECT)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)
    print(response)

    time.sleep(5)

    print("SCardEndTransaction hcard2")
    hresult = SCardEndTransaction(hcard2, SCARD_LEAVE_CARD)
    if hresult == SCARD_E_SHARING_VIOLATION:
        print(RED + " ERROR!" + NORMAL)
    if hresult != SCARD_S_SUCCESS:
        raise BaseSCardException(hresult)

    print("SCardReleaseContext hcontext2")
    hresult = SCardReleaseContext(hcontext2)
    if hresult != SCARD_S_SUCCESS:
        raise ReleaseContextException(hresult)


# 1st client
hcontext1, hcard1, reader, dwActiveProtocol = init_client()

# 3rd client
hcontext3, hcard3, reader, dwActiveProtocol = init_client()

t = threading.Thread(target=thread_transaction)
t.start()

time.sleep(1)

print("SCardReleaseContext hcontext1")
hresult = SCardReleaseContext(hcontext1)
if hresult not in (SCARD_S_SUCCESS, SCARD_E_SHARING_VIOLATION):
    raise ReleaseContextException(hresult)

time.sleep(2)

before = time.time()
print("SCardBeginTransaction hcard3 (should block)")
hresult = SCardBeginTransaction(hcard3)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
after = time.time()
delta = (after - before) * 1000
print("Delta time: %f ms" % delta)
if delta < 1000:
    print(RED + " ERROR! No blocking" + NORMAL)
else:
    print(BLUE + "Blocking: OK" + NORMAL)

SELECT = [0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00]
print("SCardTransmit hcard3")
hresult, response = SCardTransmit(hcard3, dwActiveProtocol, SELECT)
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(response)

# wait for the thread
t.join()

print("SCardReleaseContext hcontext3")
hresult = SCardReleaseContext(hcontext3)
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
