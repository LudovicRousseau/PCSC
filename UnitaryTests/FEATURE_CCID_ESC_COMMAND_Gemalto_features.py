#! /usr/bin/env python3

"""
#   FEATURE_CCID_ESC_COMMAND.py: Unitary test for FEATURE_CCID_ESC_COMMAND
#   Copyright (C) 2011-2013  Ludovic Rousseau
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
from smartcard.pcsc.PCSCPart10 import (
    SCARD_SHARE_DIRECT,
    SCARD_LEAVE_CARD,
    FEATURE_CCID_ESC_COMMAND,
    FEATURE_GET_TLV_PROPERTIES,
    getTlvProperties,
    getFeatureRequest,
    hasFeature,
)
from smartcard.Exceptions import SmartcardException

try:
    from itertools import izip
except ImportError:
    izip = zip


# http://www.usb.org/developers/docs/USB_LANGIDs.pdf
USBLangID = {
    0x0409: "English (United States)",
    0x040C: "French (Standard)",
    0x0425: "Estonian",
    0x0419: "Russian",
    0x0415: "Polish",
    0x0416: "Portuguese (Brazil)",
    0x0405: "Czech",
    0x040A: "Spanish (Traditional Sort)",
    0x041B: "Slovak",
}


def test_bit(value, bit):
    mask = 1 << bit
    return value & mask == mask


def main():
    """main"""
    card_connection = readers()[0].createConnection()
    card_connection.connect(mode=SCARD_SHARE_DIRECT, disposition=SCARD_LEAVE_CARD)

    feature_list = getFeatureRequest(card_connection)

    get_tlv_properties = hasFeature(feature_list, FEATURE_GET_TLV_PROPERTIES)
    if get_tlv_properties:
        tlv = getTlvProperties(card_connection)
        print("Reader:   ", readers()[0])
        print("IdVendor:  0x%04X" % tlv["PCSCv2_PART10_PROPERTY_wIdVendor"])
        print("IdProduct: 0x%04X" % tlv["PCSCv2_PART10_PROPERTY_wIdProduct"])

    ccid_esc_command = hasFeature(feature_list, FEATURE_CCID_ESC_COMMAND)
    if ccid_esc_command is None:
        raise Exception("FEATURE_CCID_ESC_COMMAND is not supported or allowed")

    # Proprietary command for Gemalto readers
    # This is implemented by the Gemalto Pinpad v2 and C200 readers
    firmware_features = [0x6A]
    try:
        res = card_connection.control(ccid_esc_command, firmware_features)
    except SmartcardException as ex:
        print("Failed:", ex)
        return

    print(res)
    print("LogicalLCDLineNumber (Logical number of LCD lines):", res[0])
    print("LogicalLCDRowNumber (Logical number of characters per LCD line):", res[1])
    print("LcdInfo:", res[2])
    print("  b0 indicates if scrolling available:", test_bit(res[2], 0))
    print("EntryValidationCondition:", res[3])

    print("PC/SCv2 features:")
    print(" VerifyPinStart:", test_bit(res[4], 0))
    print(" VerifyPinFinish:", test_bit(res[4], 1))
    print(" ModifyPinStart:", test_bit(res[4], 2))
    print(" ModifyPinFinish:", test_bit(res[4], 3))
    print(" GetKeyPressed:", test_bit(res[4], 4))
    print(" VerifyPinDirect:", test_bit(res[4], 5))
    print(" ModifyPinDirect:", test_bit(res[4], 6))
    print(" Abort:", test_bit(res[4], 7))

    print(" GetKey:", test_bit(res[5], 0))
    print(" WriteDisplay:", test_bit(res[5], 1))
    print(" SetSpeMessage:", test_bit(res[5], 2))
    # bits 3-7 are RFU
    # bytes 6 and 7 are RFU

    print(" bTimeOut2:", test_bit(res[8], 0))
    bListSupportedLanguages = test_bit(res[8], 1)
    print(" bListSupportedLanguages:", bListSupportedLanguages)
    if bListSupportedLanguages:
        try:
            # Reader is able to indicate the list of supported languages
            # through CCID-ESC 0x6B
            languages = card_connection.control(ccid_esc_command, [0x6B])
        except SmartcardException as ex:
            print("Failed:", ex)
        print(" ", languages)
        languages = iter(languages)
        for low, high in izip(languages, languages):
            lang_x = high * 256 + low
            try:
                lang_t = USBLangID[lang_x]
            except KeyError:
                lang_t = "unknown"
            print("  0x%04X: %s" % (lang_x, lang_t))

    print(" bNumberMessageFix:", test_bit(res[8], 2))
    print(" bPPDUSupportOverXferBlock:", test_bit(res[8], 3))
    print(" bPPDUSupportOverEscape:", test_bit(res[8], 4))
    # bits 5-7 are RFU
    # bytes 9, 10 and 11 and RFU

    print("VersionNumber:", res[12])
    print("MinimumPINSize:", res[13])
    print("MaximumPINSize:", res[14])
    Firewall = test_bit(res[15], 0)
    print("Firewall:", Firewall)
    # bits 1-7 are RFU

    if Firewall:
        print("FirewalledCommand_SW1: 0x%02X" % res[16])
        print("FirewalledCommand_SW2: 0x%02X" % res[17])
    # bytes 18-20 are RFU


if __name__ == "__main__":
    main()
