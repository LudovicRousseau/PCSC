/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : debuglog.c
	Package: pcsc lite
	Author : David Corcoran, Ludovic Rousseau
	Date   : 7/27/99, update 11 Aug, 2002
	License: Copyright (C) 1999,2002 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This handles debugging. 
	            
$Id$

********************************************************************/

#ifndef WIN32
#include <syslog.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifndef WIN32
#include "config.h"
#else
#include "../win32/win32_config.h"
#endif

#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"

// Max string size when dumping a 256 bytes longs APDU
#define DEBUG_BUF_SIZE (256*3+30)

static char DebugBuffer[DEBUG_BUF_SIZE];

static int lSuppress = DEBUGLOG_LOG_ENTRIES;
static int debug_msg_type = DEBUGLOG_NO_DEBUG;
static int debug_category = DEBUG_CATEGORY_NOTHING;

void debug_msg(const char *fmt, ...)
{
	va_list argptr;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	va_start(argptr, fmt);
#ifndef WIN32
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
#else
	vsprintf(DebugBuffer, fmt, argptr);
#endif
	va_end(argptr);

	if (debug_msg_type == DEBUGLOG_NO_DEBUG)
	{
		/*
		 * Do nothing, it hasn't been set 
		 */

	} else if (debug_msg_type & DEBUGLOG_SYSLOG_DEBUG)
	{
#ifndef WIN32
		syslog(LOG_INFO, "%s", DebugBuffer);
#else
		fprintf(stderr, "%s\n", DebugBuffer);
#endif

	} else if (debug_msg_type & DEBUGLOG_STDERR_DEBUG)
	{
		fprintf(stderr, "%s\n", DebugBuffer);

	} else if (debug_msg_type & DEBUGLOG_STDOUT_DEBUG)
	{
		fprintf(stdout, "%s\n", DebugBuffer);
	}
}	/* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	int i;
	unsigned char *c, *debug_buf_end;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strncpy(DebugBuffer, msg, sizeof(DebugBuffer) - 1);
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

	if (debug_msg_type == DEBUGLOG_NO_DEBUG)
	{
		/*
		 * Do nothing, it hasn't been set 
		 */

	} else if (debug_msg_type & DEBUGLOG_SYSLOG_DEBUG)
	{
#ifndef WIN32
		syslog(LOG_INFO, "%s", DebugBuffer);
#else
		fprintf(stderr, "%s\n", DebugBuffer);
#endif

	} else if (debug_msg_type & DEBUGLOG_STDERR_DEBUG)
	{
		fprintf(stderr, "%s\n", DebugBuffer);

	} else if (debug_msg_type & DEBUGLOG_STDOUT_DEBUG)
	{
		fprintf(stdout, "%s\n", DebugBuffer);
	}
}	/* debug_xxd */

void DebugLogSuppress(const int lSType)
{
	lSuppress = lSType;
}

void DebugLogSetLogType(const int dbgtype)
{
	debug_msg_type |= dbgtype;
}

int DebugLogSetCategory(const int dbginfo)
{
#define DEBUG_INFO_LENGTH 80
	char text[DEBUG_INFO_LENGTH];

	// use a negative number to UNset
	// typically use ~DEBUG_CATEGORY_APDU
	if (dbginfo < 0)
		debug_category &= dbginfo;
	else
		debug_category |= dbginfo;

	// set to empty string
	text[0] = '\0';

	if (debug_category & DEBUG_CATEGORY_APDU)
		strncat(text, " APDU", DEBUG_INFO_LENGTH-1-strlen(text));

	DebugLogB("Debug options:%s", text);

	return debug_category;
}

void DebugLogCategory(const int category, const char *buffer, const int len)
{
	if ((category & DEBUG_CATEGORY_APDU)
		&& (debug_category & DEBUG_CATEGORY_APDU))
		debug_xxd("APDU: ", buffer, len);

	if ((category & DEBUG_CATEGORY_SW)
		&& (debug_category & DEBUG_CATEGORY_APDU))
		debug_xxd("SW: ", buffer, len);
}

LPSTR pcsc_stringify_error(const LONG Error)
{

	static char strError[75];

	switch (Error)
	{
	case SCARD_S_SUCCESS:
		strcpy(strError, "Command successful.");
		break;
	case SCARD_E_CANCELLED:
		strcpy(strError, "Command cancelled.");
		break;
	case SCARD_E_CANT_DISPOSE:
		strcpy(strError, "Cannot dispose handle.");
		break;
	case SCARD_E_INSUFFICIENT_BUFFER:
		strcpy(strError, "Insufficient buffer.");
		break;
	case SCARD_E_INVALID_ATR:
		strcpy(strError, "Invalid ATR.");
		break;
	case SCARD_E_INVALID_HANDLE:
		strcpy(strError, "Invalid handle.");
		break;
	case SCARD_E_INVALID_PARAMETER:
		strcpy(strError, "Invalid parameter given.");
		break;
	case SCARD_E_INVALID_TARGET:
		strcpy(strError, "Invalid target given.");
		break;
	case SCARD_E_INVALID_VALUE:
		strcpy(strError, "Invalid value given.");
		break;
	case SCARD_E_NO_MEMORY:
		strcpy(strError, "Not enough memory.");
		break;
	case SCARD_F_COMM_ERROR:
		strcpy(strError, "RPC transport error.");
		break;
	case SCARD_F_INTERNAL_ERROR:
		strcpy(strError, "Unknown internal error.");
		break;
	case SCARD_F_UNKNOWN_ERROR:
		strcpy(strError, "Unknown internal error.");
		break;
	case SCARD_F_WAITED_TOO_LONG:
		strcpy(strError, "Waited too long.");
		break;
	case SCARD_E_UNKNOWN_READER:
		strcpy(strError, "Unknown reader specified.");
		break;
	case SCARD_E_TIMEOUT:
		strcpy(strError, "Command timeout.");
		break;
	case SCARD_E_SHARING_VIOLATION:
		strcpy(strError, "Sharing violation.");
		break;
	case SCARD_E_NO_SMARTCARD:
		strcpy(strError, "No smartcard inserted.");
		break;
	case SCARD_E_UNKNOWN_CARD:
		strcpy(strError, "Unknown card.");
		break;
	case SCARD_E_PROTO_MISMATCH:
		strcpy(strError, "Card protocol mismatch.");
		break;
	case SCARD_E_NOT_READY:
		strcpy(strError, "Subsystem not ready.");
		break;
	case SCARD_E_SYSTEM_CANCELLED:
		strcpy(strError, "System cancelled.");
		break;
	case SCARD_E_NOT_TRANSACTED:
		strcpy(strError, "Transaction failed.");
		break;
	case SCARD_E_READER_UNAVAILABLE:
		strcpy(strError, "Reader/s is unavailable.");
		break;
	case SCARD_W_UNSUPPORTED_CARD:
		strcpy(strError, "Card is not supported.");
		break;
	case SCARD_W_UNRESPONSIVE_CARD:
		strcpy(strError, "Card is unresponsive.");
		break;
	case SCARD_W_UNPOWERED_CARD:
		strcpy(strError, "Card is unpowered.");
		break;
	case SCARD_W_RESET_CARD:
		strcpy(strError, "Card was reset.");
		break;
	case SCARD_W_REMOVED_CARD:
		strcpy(strError, "Card was removed.");
		break;
	case SCARD_W_INSERTED_CARD:
		strcpy(strError, "Card was inserted.");
		break;
	case SCARD_E_UNSUPPORTED_FEATURE:
		strcpy(strError, "Feature not supported.");
		break;
	case SCARD_E_PCI_TOO_SMALL:
		strcpy(strError, "PCI struct too small.");
		break;
	case SCARD_E_READER_UNSUPPORTED:
		strcpy(strError, "Reader is unsupported.");
		break;
	case SCARD_E_DUPLICATE_READER:
		strcpy(strError, "Reader already exists.");
		break;
	case SCARD_E_CARD_UNSUPPORTED:
		strcpy(strError, "Card is unsupported.");
		break;
	case SCARD_E_NO_SERVICE:
		strcpy(strError, "Service not available.");
		break;
	case SCARD_E_SERVICE_STOPPED:
		strcpy(strError, "Service was stopped.");
		break;

	};

	return strError;
}

