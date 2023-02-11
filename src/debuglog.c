/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <pthread.h>

#include "pcsclite.h"
#include "misc.h"
#include "debuglog.h"
#include "sys_generic.h"

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

INTERNAL void DebugLogSetCategory(const int dbginfo)
{
	(void)dbginfo;
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
 * Max string size dumping a maximum of 2 lines of 80 characters
 */
#define DEBUG_BUF_SIZE 2048

static char LogMsgType = DEBUGLOG_NO_DEBUG;
static char LogCategory = DEBUG_CATEGORY_NOTHING;

/** default level */
static char LogLevel = PCSC_LOG_ERROR;

static signed char LogDoColor = 0;	/**< no color by default */

static void log_line(const int priority, const char *DebugBuffer,
	unsigned int rv);

/*
 * log a message with the RV value returned by the daemon
 */
void log_msg_rv(const int priority, unsigned int rv, const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if ((priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, sizeof DebugBuffer, fmt, argptr);
	va_end(argptr);

	log_line(priority, DebugBuffer, rv);
}

void log_msg(const int priority, const char *fmt, ...)
{
	char DebugBuffer[DEBUG_BUF_SIZE];
	va_list argptr;

	if ((priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, sizeof DebugBuffer, fmt, argptr);
	va_end(argptr);

	log_line(priority, DebugBuffer, -1);
} /* log_msg */

/* convert from integer rv value to a string value
 * SCARD_S_SUCCESS -> "SCARD_S_SUCCESS"
 */
const char * rv2text(unsigned int rv)
{
	const char *rv_text = NULL;
	static __thread char strError[30];

#define CASE(x) \
		case x: \
			rv_text = "rv=" #x; \
			break

	if (rv != (unsigned int)-1)
	{
		switch (rv)
		{
			CASE(SCARD_S_SUCCESS);
			CASE(SCARD_E_CANCELLED);
			CASE(SCARD_E_INSUFFICIENT_BUFFER);
			CASE(SCARD_E_INVALID_HANDLE);
			CASE(SCARD_E_INVALID_PARAMETER);
			CASE(SCARD_E_INVALID_VALUE);
			CASE(SCARD_E_NO_MEMORY);
			CASE(SCARD_E_NO_SERVICE);
			CASE(SCARD_E_NO_SMARTCARD);
			CASE(SCARD_E_NOT_TRANSACTED);
			CASE(SCARD_E_PROTO_MISMATCH);
			CASE(SCARD_E_READER_UNAVAILABLE);
			CASE(SCARD_E_SHARING_VIOLATION);
			CASE(SCARD_E_TIMEOUT);
			CASE(SCARD_E_UNKNOWN_READER);
			CASE(SCARD_E_UNSUPPORTED_FEATURE);
			CASE(SCARD_F_COMM_ERROR);
			CASE(SCARD_F_INTERNAL_ERROR);
			CASE(SCARD_W_REMOVED_CARD);
			CASE(SCARD_W_RESET_CARD);
			CASE(SCARD_W_UNPOWERED_CARD);
			CASE(SCARD_W_UNRESPONSIVE_CARD);
			CASE(SCARD_E_NO_READERS_AVAILABLE);

			default:
				(void)snprintf(strError, sizeof(strError)-1,
					"Unknown error: 0x%08X", rv);
				rv_text = strError;
		}
	}

	return rv_text;
}

static void log_line(const int priority, const char *DebugBuffer,
	unsigned int rv)
{
	if (DEBUGLOG_SYSLOG_DEBUG == LogMsgType)
		syslog(LOG_INFO, "%s", DebugBuffer);
	else
	{
		static struct timeval last_time = { 0, 0 };
		struct timeval new_time = { 0, 0 };
		struct timeval tmp;
		int delta;
		pthread_t thread_id;
		const char *rv_text = NULL;

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

		last_time = new_time;

		thread_id = pthread_self();

		rv_text = rv2text(rv);

		if (LogDoColor)
		{
			const char *color_pfx = "", *color_sfx = "\33[0m";
			const char *time_pfx = "\33[36m", *time_sfx = color_sfx;

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

#ifdef __APPLE__
#define THREAD_FORMAT "%p"
#else
#define THREAD_FORMAT "%lu"
#endif
			if (rv_text)
			{
				const char * rv_pfx = "", * rv_sfx = "";
				if (rv != SCARD_S_SUCCESS)
				{
					rv_pfx = "\33[31m"; /* Red */
					rv_sfx = "\33[0m";
				}

				printf("%s%.8d%s [" THREAD_FORMAT "] %s%s%s, %s%s%s\n",
					time_pfx, delta, time_sfx, thread_id,
					color_pfx, DebugBuffer, color_sfx,
					rv_pfx, rv_text, rv_sfx);
			}
			else
				printf("%s%.8d%s [" THREAD_FORMAT "] %s%s%s\n",
					time_pfx, delta, time_sfx, thread_id,
					color_pfx, DebugBuffer, color_sfx);
		}
		else
		{
			if (rv_text)
				printf("%.8d %s, %s\n", delta, DebugBuffer, rv_text);
			else
				printf("%.8d %s\n", delta, DebugBuffer);
		}
		fflush(stdout);
	}
} /* log_line */

static void log_xxd_always(const int priority, const char *msg,
	const unsigned char *buffer, const int len)
{
	char DebugBuffer[len*3 + strlen(msg) +1];
	int i;
	char *c;

	/* DebugBuffer is always big enough for msg */
	strcpy(DebugBuffer, msg);
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len); ++i)
	{
		/* 2 hex characters, 1 space, 1 NUL : total 4 characters */
		snprintf(c, 4, "%02X ", buffer[i]);
		c += 3;
	}

	log_line(priority, DebugBuffer, -1);
} /* log_xxd_always */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	if ((priority < LogLevel) /* log priority lower than threshold? */
		|| (DEBUGLOG_NO_DEBUG == LogMsgType))
		return;

	/* len is an error value? */
	if (len < 0)
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
		case DEBUGLOG_STDOUT_COLOR_DEBUG:
			LogMsgType = dbgtype;
			break;
		default:
			Log2(PCSC_LOG_CRITICAL, "unknown log type (%d), using stdout",
				dbgtype);
			LogMsgType = DEBUGLOG_STDOUT_DEBUG;
	}

	/* log to stdout and stdout is a tty? */
	if ((DEBUGLOG_STDOUT_DEBUG == LogMsgType && isatty(fileno(stdout)))
		|| (DEBUGLOG_STDOUT_COLOR_DEBUG == LogMsgType))
	{
		char *term;

		term = getenv("TERM");
		if (term)
		{
			const char *terms[] = { "linux", "xterm", "xterm-color", "Eterm", "rxvt", "rxvt-unicode", "xterm-256color" };
			unsigned int i;

			/* for each known color terminal */
			for (i = 0; i < COUNT_OF(terms); i++)
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
			Log1(PCSC_LOG_INFO, "debug level=info");
			break;

		case PCSC_LOG_DEBUG:
			Log1(PCSC_LOG_DEBUG, "debug level=debug");
			break;

		default:
			LogLevel = PCSC_LOG_INFO;
			Log2(PCSC_LOG_CRITICAL, "unknown level (%d), using level=info",
				level);
	}
}

INTERNAL void DebugLogSetCategory(const int dbginfo)
{
	/* use a negative number to UNset
	 * typically use ~DEBUG_CATEGORY_APDU
	 */
	if (dbginfo < 0)
		LogCategory &= dbginfo;
	else
		LogCategory |= dbginfo;

	if (LogCategory & DEBUG_CATEGORY_APDU)
		Log1(PCSC_LOG_INFO, "Debug options: APDU");
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
	vsnprintf(DebugBuffer, sizeof DebugBuffer, fmt, argptr);
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

