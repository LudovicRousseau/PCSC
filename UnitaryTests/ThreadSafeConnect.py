#! /usr/bin/env python

"""
#   ThreadSafeConnect.py : stress thread safeness of
#   SCardConnect/SCardDisconnect
#   Copyright (C) 2010  Ludovic Rousseau
"""

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


from smartcard.scard import (SCardEstablishContext, SCardReleaseContext,
    SCardListReaders, SCardConnect, SCardDisconnect,
    SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY, SCARD_LEAVE_CARD,
    SCardGetErrorMessage, SCARD_SCOPE_USER, SCARD_S_SUCCESS)
import threading

MAX_THREADS = 10
MAX_ITER = 10


def stress():
    """
    stress method
    """
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise Exception('Failed to establish context: '
            + SCardGetErrorMessage(hresult))

    hresult, readers = SCardListReaders(hcontext, [])
    if hresult != SCARD_S_SUCCESS:
        raise Exception('Failed to list readers: '
            + SCardGetErrorMessage(hresult))

    print "Using reader:", readers[0]

    for j in range(0, MAX_ITER):
        # Connect in SCARD_SHARE_SHARED mode
        hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, readers[0],
               SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
        if hresult != SCARD_S_SUCCESS:
            raise Exception('Failed to SCardConnect: '
                + SCardGetErrorMessage(hresult))

        print j,

        hresult = SCardDisconnect(hcard, SCARD_LEAVE_CARD)
        if hresult != SCARD_S_SUCCESS:
            raise Exception('Failed to SCardDisconnect: '
                + SCardGetErrorMessage(hresult))

    print

    hresult = SCardReleaseContext(hcontext)
    #print "SCardReleaseContext()", SCardGetErrorMessage(hresult)
    if hresult != SCARD_S_SUCCESS:
        raise Exception('Failed to release context: '
            + SCardGetErrorMessage(hresult))

def main():
    """
    main
    """
    threads = list()

    for i in range(0, MAX_THREADS):
        thread = threading.Thread(target=stress)
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

if __name__ == "__main__":
    main()
