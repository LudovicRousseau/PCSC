#! /usr/bin/env python3

"""
#   ThreadSafe.py : stress thread safeness
#   Copyright (C) 2010  Ludovic Rousseau
"""

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


import threading
from smartcard.scard import (
    SCardEstablishContext,
    SCardReleaseContext,
    SCardGetErrorMessage,
    SCARD_SCOPE_USER,
    SCARD_S_SUCCESS,
)
from smartcard.pcsc.PCSCExceptions import *

MAX_THREADS = 100
MAX_ITER = 10


def stress(*args):
    """
    stress method
    """
    thread = args[0]
    for j in range(0, MAX_ITER):
        print(thread, end=" ")
        hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
        if hresult != SCARD_S_SUCCESS:
            raise EstablishContextException(hresult)

        hresult = SCardReleaseContext(hcontext)
        # print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
        if hresult != SCARD_S_SUCCESS:
            raise ReleaseContextException(hresult)


def main():
    """
    main
    """

    threads = list()

    for i in range(0, MAX_THREADS):
        thread = threading.Thread(target=stress, args=(i,))
        threads.append(thread)
        print("start thread", i)
        thread.start()

    for thread in threads:
        thread.join()


if __name__ == "__main__":
    main()
