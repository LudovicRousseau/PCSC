#! /usr/bin/env python

"""
#   ThreadSafe.py : stress thread safeness
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
    SCardGetErrorMessage, SCARD_SCOPE_USER, SCARD_S_SUCCESS)
import threading

MAX_THREADS = 100
MAX_ITER = 10


def stress():
    """
    stress method
    """
    for j in range(1, MAX_ITER):
        hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
        if hresult != SCARD_S_SUCCESS:
            raise Exception('Failed to establish context: '
                + SCardGetErrorMessage(hresult))

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

    for i in range(1, MAX_THREADS):
        thread = threading.Thread(target=stress)
        threads.append(thread)
        thread.start()

    for thread in threads:
        thread.join()

if __name__ == "__main__":
    main()
