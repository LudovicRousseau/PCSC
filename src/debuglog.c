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

#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "sys_generic.h"

/* Max string size when dumping a 256 bytes longs APDU */
#define DEBUG_BUF_SIZE (256*3+30)

static int lSuppress = DEBUGLOG_LOG_ENTRIES;
static int debug_msg_type = DEBUGLOG_NO_DEBUG;
static int debug_category = DEBUG_CATEGORY_NOTHING;
static FILE *debug_file = NULL;

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

		case DEBUGLOG_FILE_DEBUG:
			if (debug_file) {
				fprintf(debug_file, "%s\n", DebugBuffer);
			} else {
				/* File not setup, do nothing. But assert to catch this in debug
				 * builds */
				assert(0);
			}
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

		case DEBUGLOG_FILE_DEBUG:
			if (debug_file) {
				fprintf(debug_file, "%s\n", DebugBuffer);
			} else {
				/* File not setup, do nothing.  But assert to catch this in debug
				 * builds. */
				assert(0);
			}
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

char* pcsc_stringify_error(long pcscError)
{
	static char strError[75];

	switch (pcscError)
	{
	case SCARD_S_SUCCESS:
		strncpy(strError, "Command successful.", sizeof(strError)-1);
		break;
	case SCARD_E_CANCELLED:
		strncpy(strError, "Command cancelled.", sizeof(strError)-1);
		break;
	case SCARD_E_CANT_DISPOSE:
		strncpy(strError, "Cannot dispose handle.", sizeof(strError)-1);
		break;
	case SCARD_E_INSUFFICIENT_BUFFER:
		strncpy(strError, "Insufficient buffer.", sizeof(strError)-1);
		break;
	case SCARD_E_INVALID_ATR:
		strncpy(strError, "Invalid ATR.", sizeof(strError)-1);
		break;
	case SCARD_E_INVALID_HANDLE:
		strncpy(strError, "Invalid handle.", sizeof(strError)-1);
		break;
	case SCARD_E_INVALID_PARAMETER:
		strncpy(strError, "Invalid parameter given.", sizeof(strError)-1);
		break;
	case SCARD_E_INVALID_TARGET:
		strncpy(strError, "Invalid target given.", sizeof(strError)-1);
		break;
	case SCARD_E_INVALID_VALUE:
		strncpy(strError, "Invalid value given.", sizeof(strError)-1);
		break;
	case SCARD_E_NO_MEMORY:
		strncpy(strError, "Not enough memory.", sizeof(strError)-1);
		break;
	case SCARD_F_COMM_ERROR:
		strncpy(strError, "RPC transport error.", sizeof(strError)-1);
		break;
	case SCARD_F_INTERNAL_ERROR:
		strncpy(strError, "Unknown internal error.", sizeof(strError)-1);
		break;
	case SCARD_F_UNKNOWN_ERROR:
		strncpy(strError, "Unknown internal error.", sizeof(strError)-1);
		break;
	case SCARD_F_WAITED_TOO_LONG:
		strncpy(strError, "Waited too long.", sizeof(strError)-1);
		break;
	case SCARD_E_UNKNOWN_READER:
		strncpy(strError, "Unknown reader specified.", sizeof(strError)-1);
		break;
	case SCARD_E_TIMEOUT:
		strncpy(strError, "Command timeout.", sizeof(strError)-1);
		break;
	case SCARD_E_SHARING_VIOLATION:
		strncpy(strError, "Sharing violation.", sizeof(strError)-1);
		break;
	case SCARD_E_NO_SMARTCARD:
		strncpy(strError, "No smartcard inserted.", sizeof(strError)-1);
		break;
	case SCARD_E_UNKNOWN_CARD:
		strncpy(strError, "Unknown card.", sizeof(strError)-1);
		break;
	case SCARD_E_PROTO_MISMATCH:
		strncpy(strError, "Card protocol mismatch.", sizeof(strError)-1);
		break;
	case SCARD_E_NOT_READY:
		strncpy(strError, "Subsystem not ready.", sizeof(strError)-1);
		break;
	case SCARD_E_SYSTEM_CANCELLED:
		strncpy(strError, "System cancelled.", sizeof(strError)-1);
		break;
	case SCARD_E_NOT_TRANSACTED:
		strncpy(strError, "Transaction failed.", sizeof(strError)-1);
		break;
	case SCARD_E_READER_UNAVAILABLE:
		strncpy(strError, "Reader/s is unavailable.", sizeof(strError)-1);
		break;
	case SCARD_W_UNSUPPORTED_CARD:
		strncpy(strError, "Card is not supported.", sizeof(strError)-1);
		break;
	case SCARD_W_UNRESPONSIVE_CARD:
		strncpy(strError, "Card is unresponsive.", sizeof(strError)-1);
		break;
	case SCARD_W_UNPOWERED_CARD:
		strncpy(strError, "Card is unpowered.", sizeof(strError)-1);
		break;
	case SCARD_W_RESET_CARD:
		strncpy(strError, "Card was reset.", sizeof(strError)-1);
		break;
	case SCARD_W_REMOVED_CARD:
		strncpy(strError, "Card was removed.", sizeof(strError)-1);
		break;
	case SCARD_W_INSERTED_CARD:
		strncpy(strError, "Card was inserted.", sizeof(strError)-1);
		break;
	case SCARD_E_UNSUPPORTED_FEATURE:
		strncpy(strError, "Feature not supported.", sizeof(strError)-1);
		break;
	case SCARD_E_PCI_TOO_SMALL:
		strncpy(strError, "PCI struct too small.", sizeof(strError)-1);
		break;
	case SCARD_E_READER_UNSUPPORTED:
		strncpy(strError, "Reader is unsupported.", sizeof(strError)-1);
		break;
	case SCARD_E_DUPLICATE_READER:
		strncpy(strError, "Reader already exists.", sizeof(strError)-1);
		break;
	case SCARD_E_CARD_UNSUPPORTED:
		strncpy(strError, "Card is unsupported.", sizeof(strError)-1);
		break;
	case SCARD_E_NO_SERVICE:
		strncpy(strError, "Service not available.", sizeof(strError)-1);
		break;
	case SCARD_E_SERVICE_STOPPED:
		strncpy(strError, "Service was stopped.", sizeof(strError)-1);
		break;

	};

	return strError;
}

#ifdef WIN32
static DWORD SYS_GetPID() {
	return GetCurrentProcessId();
}
#endif

void DebugLogSetupLogFile(const char* filename_base) {
  char *filename = NULL;
  int bufsize;

  bufsize = strlen(filename_base) + 22;
  filename = (char *)malloc(bufsize);
  if (filename == NULL) {
	  goto finish;
  }

  if ( debug_file != NULL ) {
	  fclose(debug_file);
	  debug_file = NULL;
  }

  sprintf(filename, "%s.%20d", filename_base, SYS_GetPID());
  debug_file = fopen(filename, "a");

  /* In debug mode, catch the case where we couldn't open the file */
  assert(debug_file);

finish:
  free(filename);

}
