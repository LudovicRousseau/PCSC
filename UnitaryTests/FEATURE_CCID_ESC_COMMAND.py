#! /usr/bin/env python

"""
#   FEATURE_CCID_ESC_COMMAND.py: Unitary test for FEATURE_CCID_ESC_COMMAND
#   Copyright (C) 2011  Ludovic Rousseau
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

from smartcard.System import readers
from smartcard.pcsc.PCSCPart10 import (SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD, FEATURE_CCID_ESC_COMMAND, getFeatureRequest, hasFeature)


def main():
    """ main """
    card_connection = readers()[0].createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT,
        disposition=SCARD_LEAVE_CARD)

    feature_list = getFeatureRequest(card_connection)

    ccid_esc_command = hasFeature(feature_list, FEATURE_CCID_ESC_COMMAND)
    if ccid_esc_command is None:
        raise Exception("The reader does not support FEATURE_CCID_ESC_COMMAND")

    # proprietary commands for Xiring readers
    version = [ord(c) for c in "VERSION"]
    res = card_connection.control(ccid_esc_command, version)
    print res
    print ''.join([chr(x) for x in res])

    serial = [ord(c) for c in "GET_SN"]
    res = card_connection.control(ccid_esc_command, serial)
    print res
    print ''.join([chr(x) for x in res])

if __name__ == "__main__":
    main()
