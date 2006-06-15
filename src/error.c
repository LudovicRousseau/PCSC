/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * This file is dual licenced:
 * - BSD-like, see the COPYING file
 * - GNU Lesser General Licence 2.1 or (at your option) any later version.
 *
 * $Id: debuglog.c 1827 2006-01-24 14:49:52Z rousseau $
 */

/**
 * @file
 * @brief This handles pcsc_stringify_error()
 */
 
#include <stdio.h>
#include <sys/types.h>

#include "misc.h"
#include "pcsclite.h"
#include "strlcpycat.h"

/**
 * @brief This function return a human readable text for the given PC/SC error code.
 *
 * @param[in] pcscError Error code to be translated to text.
 *
 * @return Text representing de error code passed.
 *
 * @test
 * @code
 * SCARDCONTEXT hContext;
 * LONG rv;
 * rv = SCardEstablishContext(SCARD\_SCOPE\_SYSTEM, NULL, NULL, &hContext);
 * if (rv != SCARD_S_SUCCESS)
 *     printf("SCardReleaseContext: %s (0x%lX)\n",
 *         pcsc_stringify_error(rv), rv);
 * @endcode
 */
PCSC_API char* pcsc_stringify_error(long pcscError)
{
	static char strError[75];

	switch (pcscError)
	{
	case SCARD_S_SUCCESS:
		strlcpy(strError, "Command successful.", sizeof(strError));
		break;
	case SCARD_E_CANCELLED:
		strlcpy(strError, "Command cancelled.", sizeof(strError));
		break;
	case SCARD_E_CANT_DISPOSE:
		strlcpy(strError, "Cannot dispose handle.", sizeof(strError));
		break;
	case SCARD_E_INSUFFICIENT_BUFFER:
		strlcpy(strError, "Insufficient buffer.", sizeof(strError));
		break;
	case SCARD_E_INVALID_ATR:
		strlcpy(strError, "Invalid ATR.", sizeof(strError));
		break;
	case SCARD_E_INVALID_HANDLE:
		strlcpy(strError, "Invalid handle.", sizeof(strError));
		break;
	case SCARD_E_INVALID_PARAMETER:
		strlcpy(strError, "Invalid parameter given.", sizeof(strError));
		break;
	case SCARD_E_INVALID_TARGET:
		strlcpy(strError, "Invalid target given.", sizeof(strError));
		break;
	case SCARD_E_INVALID_VALUE:
		strlcpy(strError, "Invalid value given.", sizeof(strError));
		break;
	case SCARD_E_NO_MEMORY:
		strlcpy(strError, "Not enough memory.", sizeof(strError));
		break;
	case SCARD_F_COMM_ERROR:
		strlcpy(strError, "RPC transport error.", sizeof(strError));
		break;
	case SCARD_F_INTERNAL_ERROR:
		strlcpy(strError, "Internal error.", sizeof(strError));
		break;
	case SCARD_F_UNKNOWN_ERROR:
		strlcpy(strError, "Unknown error.", sizeof(strError));
		break;
	case SCARD_F_WAITED_TOO_LONG:
		strlcpy(strError, "Waited too long.", sizeof(strError));
		break;
	case SCARD_E_UNKNOWN_READER:
		strlcpy(strError, "Unknown reader specified.", sizeof(strError));
		break;
	case SCARD_E_TIMEOUT:
		strlcpy(strError, "Command timeout.", sizeof(strError));
		break;
	case SCARD_E_SHARING_VIOLATION:
		strlcpy(strError, "Sharing violation.", sizeof(strError));
		break;
	case SCARD_E_NO_SMARTCARD:
		strlcpy(strError, "No smart card inserted.", sizeof(strError));
		break;
	case SCARD_E_UNKNOWN_CARD:
		strlcpy(strError, "Unknown card.", sizeof(strError));
		break;
	case SCARD_E_PROTO_MISMATCH:
		strlcpy(strError, "Card protocol mismatch.", sizeof(strError));
		break;
	case SCARD_E_NOT_READY:
		strlcpy(strError, "Subsystem not ready.", sizeof(strError));
		break;
	case SCARD_E_SYSTEM_CANCELLED:
		strlcpy(strError, "System cancelled.", sizeof(strError));
		break;
	case SCARD_E_NOT_TRANSACTED:
		strlcpy(strError, "Transaction failed.", sizeof(strError));
		break;
	case SCARD_E_READER_UNAVAILABLE:
		strlcpy(strError, "Reader is unavailable.", sizeof(strError));
		break;
	case SCARD_W_UNSUPPORTED_CARD:
		strlcpy(strError, "Card is not supported.", sizeof(strError));
		break;
	case SCARD_W_UNRESPONSIVE_CARD:
		strlcpy(strError, "Card is unresponsive.", sizeof(strError));
		break;
	case SCARD_W_UNPOWERED_CARD:
		strlcpy(strError, "Card is unpowered.", sizeof(strError));
		break;
	case SCARD_W_RESET_CARD:
		strlcpy(strError, "Card was reset.", sizeof(strError));
		break;
	case SCARD_W_REMOVED_CARD:
		strlcpy(strError, "Card was removed.", sizeof(strError));
		break;
	case SCARD_W_INSERTED_CARD:
		strlcpy(strError, "Card was inserted.", sizeof(strError));
		break;
	case SCARD_E_UNSUPPORTED_FEATURE:
		strlcpy(strError, "Feature not supported.", sizeof(strError));
		break;
	case SCARD_E_PCI_TOO_SMALL:
		strlcpy(strError, "PCI struct too small.", sizeof(strError));
		break;
	case SCARD_E_READER_UNSUPPORTED:
		strlcpy(strError, "Reader is unsupported.", sizeof(strError));
		break;
	case SCARD_E_DUPLICATE_READER:
		strlcpy(strError, "Reader already exists.", sizeof(strError));
		break;
	case SCARD_E_CARD_UNSUPPORTED:
		strlcpy(strError, "Card is unsupported.", sizeof(strError));
		break;
	case SCARD_E_NO_SERVICE:
		strlcpy(strError, "Service not available.", sizeof(strError));
		break;
	case SCARD_E_SERVICE_STOPPED:
		strlcpy(strError, "Service was stopped.", sizeof(strError));
		break;
	default:
		snprintf(strError, sizeof(strError)-1, "Unkown error: 0x%08lX",
			pcscError);
	};

	/* add a null byte */
	strError[sizeof(strError)] = '\0';

	return strError;
}


