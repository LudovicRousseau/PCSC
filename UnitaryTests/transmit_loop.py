#! /usr/bin/env python3

#   transmit_loop.py: call SCardTransmit in an endless loop
#   Copyright (C) 2010  Ludovic Rousseau
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

from smartcard.System import *
from smartcard.CardConnection import *

r = readers()
connection = r[0].createConnection()
connection.connect()

SELECT = [0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00]
i = 0
try:
    while True:
        print("loop:", i)
        i += 1
        data, sw1, sw2 = connection.transmit(SELECT)
        print(data)
        print("%02x %02x" % (sw1, sw2))
except KeyboardInterrupt:
    connection.disconnect()
