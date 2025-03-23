#! /usr/bin/env python3

#   SCardGetStatusChange2.py : Unitary test for SCardGetStatusChange()
#   Copyright (C) 2011  Ludovic Rousseau
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

# Check the return value of SCardGetStatusChange() for unknown readers
# Before revision 5881 SCardGetStatusChange() returned SCARD_S_SUCCESS

from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *


hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
print("SCardEstablishContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise EstablishContextException(hresult)

hresult, readers = SCardListReaders(hcontext, [])
print("SCardListReaders()", SCardGetErrorMessage(hresult))
print("PC/SC Readers:", readers)

readers = ["a", "b"]
print(readers)
readerstates = {}
for reader in readers:
    readerstates[reader] = (reader, SCARD_STATE_UNAWARE)
hresult, newstates = SCardGetStatusChange(hcontext, 10, list(readerstates.values()))
print("SCardGetStatusChange()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise BaseSCardException(hresult)
print(newstates)

hresult = SCardReleaseContext(hcontext)
print("SCardReleaseContext()", SCardGetErrorMessage(hresult))
if hresult != SCARD_S_SUCCESS:
    raise ReleaseContextException(hresult)
