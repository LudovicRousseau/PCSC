/*
 * This handles debugging.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/*
 * log message is sent to syslog or stderr depending on --foreground
 * command line argument
 *
 * Log1(priority, "text");
 *  log "text" with priority level priority
 *
 * Log2(priority, "text: %d", 1234);
 *  log "text: 1234"
 * the format string can be anything printf() can understand
 *
 * Log3(priority, "text: %d %d", 1234, 5678);
 *  log "text: 1234 5678"
 * the format string can be anything printf() can understand
 *
 * LogXxd(priority, msg, buffer, size);
 *  log "msg" + a hex dump of size bytes of buffer[]
 *
 */

#ifndef __debuglog_h__
#define __debuglog_h__

#ifdef __cplusplus
extern "C"
{
#endif

#define DEBUGLOG_LOG_ENTRIES    1
#define DEBUGLOG_IGNORE_ENTRIES 2

enum {
	DEBUGLOG_NO_DEBUG = 0,
	DEBUGLOG_SYSLOG_DEBUG,
	DEBUGLOG_STDERR_DEBUG
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

#define Log0(priority) log_msg(priority, "%s:%d:%s()", __FILE__, __LINE__, __FUNCTION__)
#define Log1(priority, fmt) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__)
#define Log2(priority, fmt, data) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define Log3(priority, fmt, data1, data2) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define Log3(priority, fmt, data1, data2) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define Log4(priority, fmt, data1, data2, data3) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3)
#define LogXxd(priority, msg, buffer, size) log_xxd(priority, msg, buffer, size)

#define DebugLogA(a) Log1(PCSC_LOG_INFO, a)
#define DebugLogB(a, b) Log2(PCSC_LOG_INFO, a, b)
#define DebugLogC(a, b,c) Log3(PCSC_LOG_INFO, a, b, c)

void log_msg(const int priority, const char *fmt, ...);
void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int size);

void DebugLogSuppress(const int);
void DebugLogSetLogType(const int);
int DebugLogSetCategory(const int);
void DebugLogCategory(const int, const unsigned char *, const int);
void DebugLogSetLevel(const int level);

char *pcsc_stringify_error(long);

#ifdef __cplusplus
}
#endif

#endif							/* __debuglog_h__ */

