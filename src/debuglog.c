/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : debuglog.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles debugging. 
	            

********************************************************************/

#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "debuglog.h"

// Max string size when dumping a 256 bytes longs APDU
#define DEBUG_BUF_SIZE (256*3+30)

static char DebugBuffer[DEBUG_BUF_SIZE];

static LONG lSuppress = DEBUGLOG_LOG_ENTRIES;
static char debug_msg_type = DEBUGLOG_NO_DEBUG;

void debug_msg(char *fmt, ...)
{
	va_list argptr;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);


	if (debug_msg_type == DEBUGLOG_NO_DEBUG) {
	  /* Do nothing, it hasn't been set */

	} else if (debug_msg_type & DEBUGLOG_SYSLOG_DEBUG) {
	  syslog(LOG_INFO, "%s", DebugBuffer);

	} else if (debug_msg_type & DEBUGLOG_STDERR_DEBUG) {
	  fprintf(stderr, "%s\n", DebugBuffer);

	} else if (debug_msg_type & DEBUGLOG_STDOUT_DEBUG) {
	  fprintf(stdout, "%s\n", DebugBuffer);
	}
} /* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	int i;
	unsigned char *c, *debug_buf_end;

	if (lSuppress != DEBUGLOG_LOG_ENTRIES)
		return;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strncpy(DebugBuffer, msg, sizeof(DebugBuffer)-1);
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

#ifdef USE_SYSLOG
	syslog(LOG_INFO, "%s", DebugBuffer);
#else
	fprintf(stderr, "%s\n", DebugBuffer);
#endif
} /* debug_xxd */

void DebugLogSuppress( LONG lSType ) {
  lSuppress = lSType;
}

void DebugLogSetLogType( LONG dbgtype ) {
  debug_msg_type |= dbgtype;
}

LPSTR pcsc_stringify_error ( LONG Error ) {

  static char strError[75];

  switch ( Error ) {
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

