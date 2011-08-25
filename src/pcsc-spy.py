#! /usr/bin/env python

#    Display PC/SC functions arguments
#    Copyright (C) 2011  Ludovic Rousseau
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

# $Id$

import os
import atexit
import sys

fifo = os.path.expanduser('~/pcsc-spy')

color_red = "\x1b[01;31m"
color_green = "\x1b[32m"
color_blue = "\x1b[34m"
color_magenta = "\x1b[35m"
color_normal = "\x1b[0m"

def cleanup():
    os.unlink(fifo)

def parse_rv(line):
    (function, rv) = line.split(':')
    if function[0] != '<':
        raise Exception("Wrong line:", line)

    return rv

def log_rv(f):
    line = f.readline().strip()
    rv = parse_rv(line)
    if not "0x00000000" in rv:
        print color_red + " =>" + rv + color_normal
    else:
        print " =>", rv

def log_in(line):
    print color_green + " i " + line + color_normal

def log_out(line):
    print color_magenta + " o " + line + color_normal

def log_in_hCard(f):
    hContext = f.readline().strip()
    log_in("hCard: %s" % hContext)

def log_in_hContext(f):
    hContext = f.readline().strip()
    log_in("hContext: %s" % hContext)

def log_out_hContext(f):
    hContext = f.readline().strip()
    log_out("hContext: %s" % hContext)

def log_in2(header, f):
    data = f.readline().strip()
    log_in("%s %s" % (header, data))

def log_out2(header, f):
    data = f.readline().strip()
    log_out("%s %s" % (header, data))

def log_name(name):
    print color_blue + name + color_normal

def SCardEstablishContext(f):
    log_name("SCardEstablishContext")
    dwScope = f.readline().strip()
    scopes = {0: 'SCARD_SCOPE_USER',
            1: 'SCARD_SCOPE_TERMINAL',
            2: 'SCARD_SCOPE_SYSTEM'}
    log_in("dwScope: %s (%s)" % (scopes[int(dwScope, 16)], dwScope))
    log_out_hContext(f)
    log_rv(f)

def SCardIsValidContext(f):
    log_name("SCardIsValidContext")
    log_in_hContext(f)
    log_rv(f)

def SCardReleaseContext(f):
    log_name("SCardReleaseContext")
    log_in_hContext(f)
    log_rv(f)

def SCardListReaders(f):
    log_name("SCardListReaders")
    log_in_hContext(f)
    log_in2("mszGroups:", f)
    log_out2("pcchReaders:", f)
    log_out2("mszReaders:", f)
    log_rv(f)

def SCardListReaderGroups(f):
    log_name("SCardListReaderGroups")
    log_in_hContext(f)
    log_in2("pcchGroups:", f)
    log_out2("pcchGroups:", f)
    log_out2("mszGroups:", f)
    log_rv(f)

def SCardGetStatusChange(f):
    log_name("SCardGetStatusChange")
    log_in_hContext(f)
    log_in2("dwTimeout:", f)
    log_in2("cReaders:", f)
    log_rv(f)

def SCardFreeMemory(f):
    log_name("SCardFreeMemory")
    log_in_hContext(f)
    log_in2("pvMem:", f)
    log_rv(f)

def SCardConnect(f):
    log_name("SCardConnect")
    log_in_hContext(f)
    log_in2("szReader", f)
    log_in2("dwShareMode", f)
    log_in2("dwPreferredProtocols", f)
    log_in2("phCard", f)
    log_in2("pdwActiveProtocol", f)
    log_out2("phCard", f)
    log_out2("pdwActiveProtocol", f)
    log_rv(f)

def SCardTransmit(f):
    log_name("SCardTransmit")
    log_in_hCard(f)
    log_in2("bSendBuffer", f)
    log_out2("bRecvBuffer", f)
    log_rv(f)

def SCardControl(f):
    log_name("SCarControl")
    log_in_hCard(f)
    log_in2("dwControlCode", f)
    log_in2("bSendBuffer", f)
    log_out2("bRecvBuffer", f)
    log_rv(f)

def SCardGetAttrib(f):
    log_name("SCardGetAttrib")
    log_in_hCard(f)
    log_in2("dwAttrId", f)
    log_out2("bAttr", f)
    log_rv(f)

def SCardSetAttrib(f):
    log_name("SCardSetAttrib")
    log_in_hCard(f)
    log_in2("dwAttrId", f)
    log_in2("bAttr", f)
    log_rv(f)

def SCardStatus(f):
    log_name("SCardStatus")
    log_in_hCard(f)
    log_in2("pcchReaderLen", f)
    log_in2("pcbAtrLen", f)
    log_out2("cchReaderLen", f)
    log_out2("mszReaderName", f)
    log_out2("dwState", f)
    log_out2("dwProtocol", f)
    log_out2("bAtr", f)
    log_rv(f)

def SCardReconnect(f):
    log_name("SCardReconnect")
    log_in_hCard(f)
    log_in2("dwShareMode", f)
    log_in2("dwPreferredProtocols", f)
    log_in2("dwInitialization", f)
    log_out2("dwActiveProtocol", f)
    log_rv(f)

def SCardDisconnect(f):
    log_name("SCardDisconnect")
    log_in_hCard(f)
    log_in2("dwDisposition", f)
    log_rv(f)

def main():
    # register clean up function
    atexit.register(cleanup)

    # create the FIFO file
    try:
        os.mkfifo(fifo)
    except (OSError):
        print "fifo %s already present. Reusing it." % fifo

    f = open(fifo, 'r')
    #f = sys.stdin

    # check version
    version = f.readline().strip()
    if version != "PCSC SPY VERSION: 1":
        print "Wrong version:", version
        return

    functions = {'SCardEstablishContext': SCardEstablishContext,
            'SCardIsValidContext': SCardIsValidContext,
            'SCardReleaseContext': SCardReleaseContext }

    line = f.readline()
    while line != '':
        # Enter function?
        if line[0] != '>':
            print "Garbage: ", line
        else:
            # dispatch
            fct = line[2:-1]
            if fct == 'SCardEstablishContext':
                SCardEstablishContext(f)
            elif fct == 'SCardReleaseContext':
                SCardReleaseContext(f)
            elif fct == 'SCardIsValidContext':
                SCardIsValidContext(f)
            elif fct == 'SCardListReaderGroups':
                SCardListReaderGroups(f)
            elif fct == 'SCardFreeMemory':
                SCardFreeMemory(f)
            elif fct == 'SCardListReaders':
                SCardListReaders(f)
            elif fct == 'SCardGetStatusChange':
                SCardGetStatusChange(f)
            elif fct == 'SCardConnect':
                SCardConnect(f)
            elif fct == 'SCardTransmit':
                SCardTransmit(f)
            elif fct == 'SCardControl':
                SCardControl(f)
            elif fct == 'SCardGetAttrib':
                SCardGetAttrib(f)
            elif fct == 'SCardSetAttrib':
                SCardSetAttrib(f)
            elif fct == 'SCardStatus':
                SCardStatus(f)
            elif fct == 'SCardReconnect':
                SCardReconnect(f)
            elif fct == 'SCardDisconnect':
                SCardDisconnect(f)
            else:
                print "Unknown function:", fct

        line = f.readline()


if __name__ == "__main__":
    main()
