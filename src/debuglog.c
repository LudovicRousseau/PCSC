/*
 * This handles debugging.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <sys/types.h>

#include "pcsclite.h"
#include "debuglog.h"
#include "sys_generic.h"
#include "strlcpycat.h"

/* Max string size when dumping a 256 bytes longs APDU
 * Should be bigger than 256*3+30 */
#define DEBUG_BUF_SIZE 2048

static int lSuppress = DEBUGLOG_LOG_ENTRIES;
static int debug_msg_type = DEBUGLOG_NO_DEBUG;
static int debug_category = DEBUG_CATEGORY_NOTHING;

void debug_msg(const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	va_start(argptr, fmt);
#ifndef WIN32
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
#else
#if HAVE_VSNPRINTF
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
#else
	vsprintf(DebugBuffer, fmt, argptr);
#endif
#endif
	va_end(argptr);

	switch(debug_msg_type) {
		case DEBUGLOG_NO_DEBUG:
		/*
		 * Do nothing, it hasn't been set 
		 */
		break;

		case DEBUGLOG_SYSLOG_DEBUG:
#ifndef WIN32
			syslog(LOG_INFO, "%s", DebugBuffer);
#else
			fprintf(stderr, "%s\n", DebugBuffer);
#endif
			break;

		case DEBUGLOG_STDERR_DEBUG:
			fprintf(stderr, "%s\n", DebugBuffer);
			break;

		case DEBUGLOG_STDOUT_DEBUG:
			fprintf(stdout, "%s\n", DebugBuffer);
			break;

		default:
			/* Unknown type. Do nothing. */
			assert(0);
	}
}	/* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	int i;
	char *c;
        char *debug_buf_end;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strlcpy(DebugBuffer, msg, sizeof(DebugBuffer));
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

	switch( debug_msg_type ) {
		case DEBUGLOG_NO_DEBUG:
		/*
		 * Do nothing, it hasn't been set 
		 */
		break;

		case DEBUGLOG_SYSLOG_DEBUG:
#ifndef WIN32
		syslog(LOG_INFO, "%s", DebugBuffer);
#else
		fprintf(stderr, "%s\n", DebugBuffer);
#endif
		break;

		case DEBUGLOG_STDERR_DEBUG:
		fprintf(stderr, "%s\n", DebugBuffer);
		break;

		case DEBUGLOG_STDOUT_DEBUG:
		fprintf(stdout, "%s\n", DebugBuffer);
		break;

		default:
			/* Unknown type - do nothing */
			assert(0);
	}
}	/* debug_xxd */

void DebugLogSuppress(const int lSType)
{
	lSuppress = lSType;
}

void DebugLogSetLogType(const int dbgtype)
{
	debug_msg_type = dbgtype;
}

int DebugLogSetCategory(const int dbginfo)
{
#define DEBUG_INFO_LENGTH 80
	char text[DEBUG_INFO_LENGTH];

	/* use a negative number to UNset
	 * typically use ~DEBUG_CATEGORY_APDU
	 */
	if (dbginfo < 0)
		debug_category &= dbginfo;
	else
		debug_category |= dbginfo;

	/* set to empty string */
	text[0] = '\0';

	if (debug_category & DEBUG_CATEGORY_APDU)
		strlcat(text, " APDU", sizeof(text));

	DebugLogB("Debug options:%s", text);

	return debug_category;
}

void DebugLogCategory(const int category, const unsigned char *buffer,
	const int len)
{
	if ((category & DEBUG_CATEGORY_APDU)
		&& (debug_category & DEBUG_CATEGORY_APDU))
		debug_xxd("APDU: ", (const unsigned char *)buffer, len);

	if ((category & DEBUG_CATEGORY_SW)
		&& (debug_category & DEBUG_CATEGORY_APDU))
		debug_xxd("SW: ", (const unsigned char *)buffer, len);
}

char* pcsc_stringify_error(long pcscError)
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
		strlcpy(strError, "Unknown internal error.", sizeof(strError));
		break;
	case SCARD_F_UNKNOWN_ERROR:
		strlcpy(strError, "Unknown internal error.", sizeof(strError));
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
		strlcpy(strError, "Reader/s is unavailable.", sizeof(strError));
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

