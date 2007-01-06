/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles debugging for pcscd.
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
#include "misc.h"
#include "debuglog.h"
#include "sys_generic.h"
#include "strlcpycat.h"

/**
 * Max string size when dumping a 256 bytes longs APDU
 * Should be bigger than 256*3+30
 */
#define DEBUG_BUF_SIZE 2048

static char LogSuppress = DEBUGLOG_LOG_ENTRIES;
static char LogMsgType = DEBUGLOG_NO_DEBUG;
static char LogCategory = DEBUG_CATEGORY_NOTHING;

/* default level is a bit verbose to be backward compatible */
static char LogLevel = PCSC_LOG_INFO;

static signed char LogDoColor = 0;	/* no color by default */

void log_msg(const int priority, const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if ((LogSuppress != DEBUGLOG_LOG_ENTRIES)
		|| (priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
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

#ifndef WIN32
	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
	{
		if (LogDoColor)
		{
			const char *color_pfx = "", *color_sfx = "\33[0m";

			switch (priority)
			{
				case PCSC_LOG_CRITICAL:
					color_pfx = "\33[01;31m"; /* bright + Red */
					break;

				case PCSC_LOG_ERROR:
					color_pfx = "\33[35m"; /* Magenta */
					break;

				case PCSC_LOG_INFO:
					color_pfx = "\33[34m"; /* Blue */
					break;

				case PCSC_LOG_DEBUG:
					color_pfx = ""; /* normal (black) */
					color_sfx = "";
					break;
			}
			fprintf(stderr, "%s%s%s\n", color_pfx, DebugBuffer, color_sfx);
		}
		else
			fprintf(stderr, "%s\n", DebugBuffer);
	}
#else
	fprintf(stderr, "%s\n", DebugBuffer);
#endif
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	int i;
	char *c;
	char *debug_buf_end;

	if ((LogSuppress != DEBUGLOG_LOG_ENTRIES)
		|| (priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strlcpy(DebugBuffer, msg, sizeof(DebugBuffer));
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += 3;
	}

	/* the buffer is too small so end it with "..." */
	if ((c >= debug_buf_end) && (i < len))
		c[-3] = c[-2] = c[-1] = '.';

#ifndef WIN32
	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
#endif
		fprintf(stderr, "%s\n", DebugBuffer);
} /* log_xxd */

#ifdef PCSCD
void DebugLogSuppress(const int lSType)
{
	LogSuppress = lSType;
}
#endif

void DebugLogSetLogType(const int dbgtype)
{
	switch (dbgtype)
	{
		case DEBUGLOG_NO_DEBUG:
		case DEBUGLOG_SYSLOG_DEBUG:
		case DEBUGLOG_STDERR_DEBUG:
			LogMsgType = dbgtype;
			break;
		default:
			Log2(PCSC_LOG_CRITICAL, "unknown log type (%d), using stderr",
				dbgtype);
			LogMsgType = DEBUGLOG_STDERR_DEBUG;
	}

	/* no color under Windows */
#ifndef WIN32
	/* log to stderr and stderr is a tty? */
	if (DEBUGLOG_STDERR_DEBUG == LogMsgType && isatty(fileno(stderr)))
	{
		const char *terms[] = { "linux", "xterm", "xterm-color", "Eterm", "rxvt", "rxvt-unicode" };
		char *term;

		term = getenv("TERM");
		if (term)
		{
			unsigned int i;

			/* for each known color terminal */
			for (i = 0; i < sizeof(terms) / sizeof(terms[0]); i++)
			{
				/* we found a supported term? */
				if (0 == strcmp(terms[i], term))
				{
					LogDoColor = 1;
					break;
				}
			}
		}
	}
#endif
}

void DebugLogSetLevel(const int level)
{
	LogLevel = level;
	switch (level)
	{
		case PCSC_LOG_CRITICAL:
		case PCSC_LOG_ERROR:
			/* do not log anything */
			break;

		case PCSC_LOG_INFO:
			Log1(PCSC_LOG_INFO, "debug level=notice");
			break;

		case PCSC_LOG_DEBUG:
			Log1(PCSC_LOG_DEBUG, "debug level=debug");
			break;

		default:
			LogLevel = PCSC_LOG_INFO;
			Log2(PCSC_LOG_CRITICAL, "unknown level (%d), using level=notice",
				level);
	}
}

INTERNAL int DebugLogSetCategory(const int dbginfo)
{
#define DEBUG_INFO_LENGTH 80
	char text[DEBUG_INFO_LENGTH];

	/* use a negative number to UNset
	 * typically use ~DEBUG_CATEGORY_APDU
	 */
	if (dbginfo < 0)
		LogCategory &= dbginfo;
	else
		LogCategory |= dbginfo;

	/* set to empty string */
	text[0] = '\0';

	if (LogCategory & DEBUG_CATEGORY_APDU)
		strlcat(text, " APDU", sizeof(text));

	Log2(PCSC_LOG_INFO, "Debug options:%s", text);

	return LogCategory;
}

INTERNAL void DebugLogCategory(const int category, const unsigned char *buffer,
	const int len)
{
	if ((category & DEBUG_CATEGORY_APDU)
		&& (LogCategory & DEBUG_CATEGORY_APDU))
		log_xxd(PCSC_LOG_INFO, "APDU: ", (const unsigned char *)buffer, len);

	if ((category & DEBUG_CATEGORY_SW)
		&& (LogCategory & DEBUG_CATEGORY_APDU))
		log_xxd(PCSC_LOG_INFO, "SW: ", (const unsigned char *)buffer, len);
}

/*
 * old function supported for backward object code compatibility
 * defined only for pcscd
 */
#ifdef PCSCD
void debug_msg(const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if ((LogSuppress != DEBUGLOG_LOG_ENTRIES)
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
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

#ifndef WIN32
	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
#endif
		fprintf(stderr, "%s\n", DebugBuffer);
} /* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	log_xxd(PCSC_LOG_ERROR, msg, buffer, len);
} /* debug_xxd */
#endif

