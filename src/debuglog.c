/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
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
#include <sys/time.h>
#include <time.h>

#include "pcsclite.h"
#include "misc.h"
#include "debuglog.h"
#include "sys_generic.h"
#include "strlcpycat.h"

#ifdef NO_LOG

void log_msg(const int priority, const char *fmt, ...)
{
	(void)priority;
	(void)fmt;
}

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	(void)priority;
	(void)msg;
	(void)buffer;
	(void)len;
}

void DebugLogSetLogType(const int dbgtype)
{
	(void)dbgtype;
}

void DebugLogSetLevel(const int level)
{
	(void)level;
}

INTERNAL int DebugLogSetCategory(const int dbginfo)
{
	(void)dbginfo;

	return 0;
}

INTERNAL void DebugLogCategory(const int category, const unsigned char *buffer,
	const int len)
{
	(void)category;
	(void)buffer;
	(void)len;
}

#else

/**
 * Max string size when dumping a 256 bytes longs APDU
 * Should be bigger than 256*3+30
 */
#define DEBUG_BUF_SIZE 2048

static char LogMsgType = DEBUGLOG_NO_DEBUG;
static char LogCategory = DEBUG_CATEGORY_NOTHING;

/** default level */
static char LogLevel = PCSC_LOG_ERROR;

static signed char LogDoColor = 0;	/**< no color by default */

static void log_line(const int priority, const char *DebugBuffer);

void log_msg(const int priority, const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if ((priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);

	log_line(priority, DebugBuffer);
} /* log_msg */

static void log_line(const int priority, const char *DebugBuffer)
{
	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
	{
		if (LogDoColor)
		{
			const char *color_pfx = "", *color_sfx = "\33[0m";
			const char *time_pfx = "\33[36m", *time_sfx = color_sfx;
			static struct timeval last_time = { 0, 0 };
			struct timeval new_time = { 0, 0 };
			struct timeval tmp;
			int delta;

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

			gettimeofday(&new_time, NULL);
			if (0 == last_time.tv_sec)
				last_time = new_time;

			tmp.tv_sec = new_time.tv_sec - last_time.tv_sec;
			tmp.tv_usec = new_time.tv_usec - last_time.tv_usec;
			if (tmp.tv_usec < 0)
			{
				tmp.tv_sec--;
				tmp.tv_usec += 1000000;
			}
			if (tmp.tv_sec < 100)
				delta = tmp.tv_sec * 1000000 + tmp.tv_usec;
			else
				delta = 99999999;

			printf("%s%.8d%s %s%s%s\n", time_pfx, delta, time_sfx,
				color_pfx, DebugBuffer, color_sfx);
			last_time = new_time;
		}
		else
			puts(DebugBuffer);
	}
} /* log_msg */

static void log_xxd_always(const int priority, const char *msg,
	const unsigned char *buffer, const int len)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	int i;
	char *c;
	char *debug_buf_end;

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

	log_line(priority, DebugBuffer);
} /* log_xxd_always */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	if ((priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	log_xxd_always(priority, msg, buffer, len);
} /* log_xxd */

void DebugLogSetLogType(const int dbgtype)
{
	switch (dbgtype)
	{
		case DEBUGLOG_NO_DEBUG:
		case DEBUGLOG_SYSLOG_DEBUG:
		case DEBUGLOG_STDOUT_DEBUG:
			LogMsgType = dbgtype;
			break;
		default:
			Log2(PCSC_LOG_CRITICAL, "unknown log type (%d), using stdout",
				dbgtype);
			LogMsgType = DEBUGLOG_STDOUT_DEBUG;
	}

	/* log to stdout and stdout is a tty? */
	if (DEBUGLOG_STDOUT_DEBUG == LogMsgType && isatty(fileno(stdout)))
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
		log_xxd_always(PCSC_LOG_INFO, "APDU: ", buffer, len);

	if ((category & DEBUG_CATEGORY_SW)
		&& (LogCategory & DEBUG_CATEGORY_APDU))
		log_xxd_always(PCSC_LOG_INFO, "SW: ", buffer, len);
}

/*
 * old function supported for backward object code compatibility
 * defined only for pcscd
 */
#ifdef PCSCD
void debug_msg(const char *fmt, ...);
void debug_msg(const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if (DEBUGLOG_NO_DEBUG == LogMsgType)
		return;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);

	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
		puts(DebugBuffer);
} /* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len);
void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	log_xxd(PCSC_LOG_ERROR, msg, buffer, len);
} /* debug_xxd */
#endif

#endif	/* NO_LOG */

