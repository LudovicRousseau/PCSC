#!/usr/bin/env python3

# Check the return value of SCardConnect() on a reader already used in
# SCARD_SHARE_EXCLUSIVE mode

"""
# Copyright (c) 2010 Jean-Luc Giraud (jlgiraud@mac.com)
# All rights reserved.
"""

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import sys
from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
from smartcard.Exceptions import NoReadersException


def Connect(mode):
    """Connect"""
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    hresult, readers = SCardListReaders(hcontext, [])
    if hresult != SCARD_S_SUCCESS:
        raise ListReadersException(hresult)
    print("PC/SC Readers:", readers)
    if len(readers) <= 0:
        raise NoReadersException()
    reader = readers[0]
    print("Using reader:", reader)

    hresult, hcard, dwActiveProtocol = SCardConnect(
        hcontext, reader, mode, SCARD_PROTOCOL_ANY
    )
    return hresult, hcontext, hcard, reader


def ConnectWithReader(readerName, mode):
    """ConnectWithReader"""
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    hresult, hcard, dwActiveProtocol = SCardConnect(
        hcontext, readerName, mode, SCARD_PROTOCOL_ANY
    )
    return hresult, hcontext, hcard


def main():
    """docstring for main"""

    hresult, hcontext1, hcard1, readerName = Connect(SCARD_SHARE_EXCLUSIVE)
    if hresult != SCARD_S_SUCCESS:
        print("Test1: Error creating first exclusive connection % x" % hresult)
        return

    hresult, hcontext2, hcardTest2 = ConnectWithReader(readerName, SCARD_SHARE_SHARED)
    if hresult != SCARD_E_SHARING_VIOLATION:
        print(
            "Test1: Expected %s (SCARD_E_SHARING_VIOLATION) but got %s"
            % (
                SCardGetErrorMessage(SCARD_E_SHARING_VIOLATION),
                SCardGetErrorMessage(hresult),
            )
        )
        return

    SCardDisconnect(hcard1, 0)

    hresult, hcontext1, hcard1 = ConnectWithReader(readerName, SCARD_SHARE_SHARED)
    if hresult != SCARD_S_SUCCESS:
        print("Testt2: Error creating first shared connection % x" % hresult)
        return

    hresult, hcontext2, hcardTest2 = ConnectWithReader(
        readerName, SCARD_SHARE_EXCLUSIVE
    )
    if hresult != SCARD_E_SHARING_VIOLATION:
        print(
            "Test2: Expected %s (SCARD_E_SHARING_VIOLATION) but got %s"
            % (
                SCardGetErrorMessage(SCARD_E_SHARING_VIOLATION),
                SCardGetErrorMessage(hresult),
            )
        )
        sys.exit(1)
    print("Test successful")


if __name__ == "__main__":
    main()
