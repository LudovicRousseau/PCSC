#! /usr/bin/env python3

"""
stress_apdu.py: send an apdu in loop
Copyright (C) 2010  Ludovic Rousseau
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

import sys
from time import ctime, time

from smartcard.System import readers


def stress(reader):
    """stress"""
    print("Using:", reader)

    # define the APDUs used in this script
    SELECT = [
        0x00,
        0xA4,
        0x04,
        0x00,
        0x0A,
        0xA0,
        0x00,
        0x00,
        0x00,
        0x62,
        0x03,
        0x01,
        0x0C,
        0x06,
        0x01,
    ]
    COMMAND = [0x00, 0x00, 0x00, 0x00]

    connection = reader.createConnection()
    connection.connect()

    data, sw1, sw2 = connection.transmit(SELECT)
    print(data)
    print(f"Select Applet: {sw1:02X} {sw2:02X}")

    cpt = 0
    while True:
        before = time()
        data, sw1, sw2 = connection.transmit(COMMAND)
        after = time()
        print(data)
        delta = after - before
        print(f"{cpt} Command: {sw1:02X} {sw2:02X}, delta:{delta}")
        if delta > 1:
            sys.stderr.write(ctime() + f" {delta}\n")
        cpt += 1


if __name__ == "__main__":

    # get all the available readers
    readers = readers()
    print("Available readers:")
    for i, r in enumerate(readers):
        print("{i}: {r}")

    try:
        i = int(sys.argv[1])
    except IndexError:
        i = 0

    stress(readers[i])
