#! /usr/bin/env python3

#   SCardCancel2.py : Unitary test for SCardCancel()
#   Copyright (C) 2010  Jan Rochat
#   https://bugs.launchpad.net/ubuntu/+source/pcsc-lite/+bug/647545/comments/6
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

# SCardCancel() should do nothing if no cancellable call is ongoing
# bug fixed in revision 5344

from smartcard.scard import *

try:
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise Exception('Failed to establish context : ' +
                        SCardGetErrorMessage(hresult))
    print('Context established!')

    try:
        hresult = SCardCancel(hcontext)
        if hresult != SCARD_S_SUCCESS:
            raise Exception('Failed to cancel context : ' +
                            SCardGetErrorMessage(hresult))
        print('context canceled')

    finally:
        hresult = SCardReleaseContext(hcontext)
        if hresult != SCARD_S_SUCCESS:
            raise Exception('Failed to release context: ' +
                            SCardGetErrorMessage(hresult))
        print('Released context.')

except Exception as message:
    print("Exception:", message)
