/*
 * This handles debugging.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/*
 * log message is sent to syslog, stdout or stderr depending on --debug
 * command line argument
 *
 * DebugLogA("text");
 *  log "text"
 *
 * DebugLogB("text: %d", 1234);
 *  log "text: 1234"
 * the format string can be anything printf() can understand
 *
 * DebugLogC("text: %d %d", 1234, 5678);
 *  log "text: 1234 5678"
 * the format string can be anything printf() can understand
 *
 * DebugXxd(msg, buffer, size);
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

typedef enum {
	DEBUGLOG_NO_DEBUG = 0,
	DEBUGLOG_SYSLOG_DEBUG,
	DEBUGLOG_STDERR_DEBUG,
	DEBUGLOG_STDOUT_DEBUG
} DebugLogType;

#define DEBUG_CATEGORY_NOTHING  0
#define DEBUG_CATEGORY_APDU     1 
#define DEBUG_CATEGORY_SW       2 

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#ifndef NO_PCSC_DEBUG
#define DebugLogA(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DebugLogB(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DebugLogC(fmt, data1, data2) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define DebugXxd(msg, buffer, size) debug_xxd(msg, buffer, size)
#else
#define DebugLogA(fmt)
#define DebugLogB(fmt, data)
#define DebugLogC(fmt, data1, data2)
#define DebugXxd(msg, buffer, size)
#endif

void debug_msg(const char *fmt, ...);
void debug_xxd(const char *msg, const unsigned char *buffer, const int size);

void DebugLogSuppress(const int);
void DebugLogSetLogType(const int);
int DebugLogSetCategory(const int);
void DebugLogCategory(const int, const unsigned char *, const int);

char *pcsc_stringify_error(long);

#ifdef __cplusplus
}
#endif

#endif							/* __debuglog_h__ */

