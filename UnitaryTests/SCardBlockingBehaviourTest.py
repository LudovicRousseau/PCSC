#!/usr/bin/env python3

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
import threading
import time
from smartcard.scard import *
from smartcard.pcsc.PCSCExceptions import *
from smartcard.Exceptions import NoReadersException


# Global variable used by test functions to acknowledge
# that they do not get blocked
unblocked = False


def check(testFunctionName, hresult, duration):
    """ check """
    if hresult != SCARD_S_SUCCESS:
        print("%s failed: %s" % (testFunctionName, SCardGetErrorMessage(hresult)))
        print('Failure for "Sharing violation" are OK for non blocking calls')
    print("%s finished after %ss" % (testFunctionName, duration))


def SCardReconnectTest(hcontextTest, hcardTest, readerName):
    """ SCardReconnectTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    before = time.time()
    hresult, dwDisposition = SCardReconnect(hcardTest, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T0, SCARD_LEAVE_CARD)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


def SCardGetAttribTest(hcontextTest, hcardTest, readerName):
    """ SCardGetAttribTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    before = time.time()
    hresult, attrib = SCardGetAttrib(hcardTest, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


def SCardTransmitTest(hcontextTest, hcardTest, readerName):
    """ SCardTransmitTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    SELECT = [0xA0, 0xA4, 0x00, 0x00, 0x02]
    DF_TELECOM = [0x7F, 0x10]
    before = time.time()
    hresult, response = SCardTransmit(hcardTest, SCARD_PCI_T0, SELECT + DF_TELECOM)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


def SCardStatusTest(hcontextTest, hcardTest, readerName):
    """ SCardStatusTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    before = time.time()
    hresult, reader, state, protocol, atr = SCardStatus(hcardTest)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


def SCardConnectTest(hcontextTest, hcardTest, readerName):
    """ SCardConnectTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    before = time.time()
    # Connect in SCARD_SHARE_SHARED mode
    hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, readerName,
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


def SCardBeginTransactionTest(hcontextTest, hcardTest, readerName):
    """ SCardBeginTransactionTest """
    global unblocked
    testFunctionName = sys._getframe().f_code.co_name
    print("Test thread for %s" % testFunctionName)
    before = time.time()
    hresult = SCardBeginTransaction(hcardTest)
    if hresult != SCARD_S_SUCCESS:
        print("%s failed: %s" % (testFunctionName, SCardGetErrorMessage(hresult)))
        print('Failure for "Sharing violation" are OK for non blocking calls')
    hresult = SCardEndTransaction(hcardTest, SCARD_LEAVE_CARD)
    check(testFunctionName, hresult, time.time() - before)
    unblocked = True


# def TemplateTest(hcontextTest, hcardTest, readerName):
#    global unblocked
#    testFunctionName = sys._getframe().f_code.co_name
#    print("Test thread for %s" % testFunctionName)
#    before = time.time()
#    hresult, attrib = Template(hcardTest, SCARD_ATTR_DEVICE_FRIENDLY_NAME_A)
#    check(testFunctionName, hresult, time.time() - before)
#    unblocked = True


# Format:
# 'functioname' : [TestFunction, ShouldBlock?]

tests_SnowLeopard = {
        'SCardReconnect'        : [SCardReconnectTest, False],
        'SCardGetAttrib'        : [SCardGetAttribTest, False],
        'SCardTransmit'         : [SCardTransmitTest, False],
        'SCardStatus'           : [SCardStatusTest, False],
        'SCardConnect'          : [SCardConnectTest, True],
        'SCardBeginTransaction' : [SCardBeginTransactionTest, True]}

tests_Win7 = {
        'SCardReconnect'        : [SCardReconnectTest, True],
        'SCardGetAttrib'        : [SCardGetAttribTest, False],
        'SCardTransmit'         : [SCardTransmitTest, True],
        'SCardStatus'           : [SCardStatusTest, True],
        'SCardConnect'          : [SCardConnectTest, True],
        'SCardBeginTransaction' : [SCardBeginTransactionTest, True]}

tests_Linux = tests_Win7

# Windows should be the reference implementation
tests = tests_Linux


def Connect(index=0):
    """docstring for Connect"""
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    hresult, readers = SCardListReaders(hcontext, [])
    if hresult != SCARD_S_SUCCESS:
        raise ListReadersException(hresult)
    print('PC/SC Readers:', readers)
    if (len(readers) <= 0):
        raise NoReadersException()
    reader = readers[index]
    print("Using reader:", reader)

    # Connect in SCARD_SHARE_SHARED mode
    hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, reader,
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
    if hresult != SCARD_S_SUCCESS:
        print("SCardConnect failed")
        raise BaseSCardException(hresult)
    return hcontext, hcard, reader


def ConnectWithReader(readerName):
    """docstring for Connect"""
    hresult, hcontext = SCardEstablishContext(SCARD_SCOPE_USER)
    if hresult != SCARD_S_SUCCESS:
        raise EstablishContextException(hresult)

    # Connect in SCARD_SHARE_SHARED mode
    hresult, hcard, dwActiveProtocol = SCardConnect(hcontext, readerName,
        SCARD_SHARE_SHARED, SCARD_PROTOCOL_ANY)
    if hresult != SCARD_S_SUCCESS:
        print("SCardConnect failed")
        raise BaseSCardException(hresult)
    return hcontext, hcard


def main():
    """docstring for main"""
    global unblocked

    RED = "\033[0;31m"
    BLUE = "\033[0;34m"
    NORMAL = "\033[00m"

    # if len(sys.argv) < 4:
    #    print(usage)
    #    sys.exit(1)
    # Allow to specify test name
    # Options:
    #   -m : run manually (independent processes)

    index = 0
    if len(sys.argv) > 1:
        index = int(sys.argv[1])
    hcontext, hcard, readerName = Connect(index)
    # Creating the test handles here:
    # the test thread can't create them as
    # doing it may block on another call
    hcontextTest, hcardTest = ConnectWithReader(readerName)

    for testTarget in tests.keys():
        # Start test function in a new thread
        SCardBeginTransaction(hcard)
        unblocked = False
        print()
        print("Testing %s, expecting" % testTarget, end=' ')
        methodName, shouldBlock = tests[testTarget]
        if shouldBlock:
            print("blocking")
        else:
            print("non blocking")
        thread = threading.Thread(target=methodName, args=(hcontextTest, hcardTest, readerName))
        thread.start()
        # print("Thread started")
        time.sleep(1)
        if (unblocked and shouldBlock) or ((not unblocked) and (not shouldBlock)):
            failed = True
        else:
            failed = False
        SCardEndTransaction(hcard, SCARD_LEAVE_CARD)

        # wait for the thread to finish
        thread.join()

        if failed:
            print(RED + "Test for " + testTarget + " FAILED!" + NORMAL)
        else:
            print(BLUE + "Test for " + testTarget + " succeeded" + NORMAL)

if __name__ == '__main__':
    main()
