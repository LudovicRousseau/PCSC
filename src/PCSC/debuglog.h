/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 1999-2022
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
 * @brief This handles debugging.
 *
 * @note log message is sent to syslog or stderr depending on --foreground
 * command line argument
 *
 * @code
 * Log1(priority, "text");
 *  log "text" with priority level priority
 * Log2(priority, "text: %d", 1234);
 *  log "text: 1234"
 * the format string can be anything printf() can understand
 * Log3(priority, "text: %d %d", 1234, 5678);
 *  log "text: 1234 5678"
 * the format string can be anything printf() can understand
 * LogXxd(priority, msg, buffer, size);
 *  log "msg" + a hex dump of size bytes of buffer[]
 * @endcode
 */

#ifndef __debuglog_h__
#define __debuglog_h__

#ifndef PCSC_API
#define PCSC_API
#endif

enum {
	DEBUGLOG_NO_DEBUG = 0,
	DEBUGLOG_SYSLOG_DEBUG,
	DEBUGLOG_STDOUT_DEBUG,
	DEBUGLOG_STDOUT_COLOR_DEBUG
};

#define DEBUG_CATEGORY_NOTHING  0
#define DEBUG_CATEGORY_APDU     1
#define DEBUG_CATEGORY_SW       2

enum {
	PCSC_LOG_DEBUG = 0,
	PCSC_LOG_INFO,
	PCSC_LOG_ERROR,
	PCSC_LOG_CRITICAL
};

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#ifndef __GNUC__
#define __attribute__(x) /*nothing*/
#endif

#ifdef NO_LOG

#define Log0(priority) do { } while(0)
#define Log1(priority, fmt) do { } while(0)
#define Log2(priority, fmt, data) do {(void)priority; } while(0)
#define Log3(priority, fmt, data1, data2) do {int _p = priority; (void)_p; } while(0)
#define Log4(priority, fmt, data1, data2, data3) do { } while(0)
#define LogRv4(priority, rv, fmt, data1, data2) do { } while(0)
#define Log5(priority, fmt, data1, data2, data3, data4) do { } while(0)
#define Log9(priority, fmt, data1, data2, data3, data4, data5, data6, data7, data8) do { } while(0)
#define LogXxd(priority, msg, buffer, size) do { } while(0)
#define rv2text(rv) ""

#define DebugLogA(a)
#define DebugLogB(a, b)
#define DebugLogC(a, b,c)

#else

#define Log0(priority) log_msg(priority, "%s:%d:%s()", __FILE__, __LINE__, __FUNCTION__)
#define Log1(priority, fmt) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__)
#define Log2(priority, fmt, data) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define Log3(priority, fmt, data1, data2) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define Log4(priority, fmt, data1, data2, data3) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3)
#define LogRv4(priority, rv, fmt, data1, data2) log_msg_rv(priority, rv, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define Log5(priority, fmt, data1, data2, data3, data4) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3, data4)
#define Log9(priority, fmt, data1, data2, data3, data4, data5, data6, data7, data8) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3, data4, data5, data6, data7, data8)
#define LogXxd(priority, msg, buffer, size) log_xxd(priority, msg, buffer, size)
const char * rv2text(unsigned int rv);

#define DebugLogA(a) Log1(PCSC_LOG_INFO, a)
#define DebugLogB(a, b) Log2(PCSC_LOG_INFO, a, b)
#define DebugLogC(a, b,c) Log3(PCSC_LOG_INFO, a, b, c)

#endif	/* NO_LOG */

PCSC_API void log_msg_rv(const int priority, unsigned int rv, const char *fmt, ...)
	__attribute__((format(printf, 3, 4)));

PCSC_API void log_msg(const int priority, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

PCSC_API void log_xxd(const int priority, const char *msg,
	const unsigned char *buffer, const int size);

void DebugLogSuppress(const int);
void DebugLogSetLogType(const int);
void DebugLogSetCategory(const int);
void DebugLogCategory(const int, const unsigned char *, const int);
PCSC_API void DebugLogSetLevel(const int level);

#endif							/* __debuglog_h__ */

