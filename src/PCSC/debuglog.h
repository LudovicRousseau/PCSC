/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
    Title  : debuglog.h
    Package: pcsc lite
	Authors: David Corcoran, Ludovic Rousseau
	Date   : 7/27/99, updated 11 Aug, 2002
	License: Copyright (C) 1999,2002 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This handles debugging. 

$Id$

********************************************************************/

/*
 * DebugLogA("text");
 *  send "text" to syslog if USE_SYSLOG is defined
 *  print to stderr "text" if USE_SYSLOG is NOT defined
 *
 * DebugLogB("text: %d", 1234)
 *  send "text: 1234" to syslog if USE_SYSLOG is defined
 *  print to stderr "text: 1234" is USE_SYSLOG is NOT defined
 * the format string can be anything printf() can understand
 *
 * DebugXxd(msg, buffer, size)
 *  send to syslog (if USE_SYSLOG is defined) or print to stderr
 *  "msg" + a hex dump of size bytes of buffer[]
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

#define DEBUGLOG_NO_DEBUG       0
#define DEBUGLOG_SYSLOG_DEBUG   1
#define DEBUGLOG_STDERR_DEBUG   2
#define DEBUGLOG_STDOUT_DEBUG   4

#define DEBUG_CATEGORY_NOTHING  0
#define DEBUG_CATEGORY_APDU     1 
#define DEBUG_CATEGORY_SW       2 

#ifdef PCSC_DEBUG
#define DebugLogA(fmt) debug_msg("%s:%d " fmt, __FILE__, __LINE__)
#define DebugLogB(fmt, data) debug_msg("%s:%d " fmt, __FILE__, __LINE__, data)
#define DebugLogC(fmt, data1, data2) debug_msg("%s:%d " fmt, __FILE__, __LINE__, data1, data2)
#define DebugXxd(msg, buffer, size) debug_xxd(msg, buffer, size)
#else
#define DebugLogA(fmt)
#define DebugLogB(fmt, data)
#define DebugLogC(fmt, data1, data2)
#define DebugXxd(msg, buffer, size)
#endif

	void debug_msg(const char *fmt, ...);
	void debug_xxd(const char *msg, const unsigned char *buffer,
		const int size);

	void DebugLogSuppress(const int);
	void DebugLogSetLogType(const int);
	int DebugLogSetCategory(const int);
	void DebugLogCategory(const int, const char *, const int);

	char *pcsc_stringify_error(const long);

#ifdef __cplusplus
}
#endif

#endif							/* __debuglog_h__ */

