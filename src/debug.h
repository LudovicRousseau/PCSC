/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 1999-2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: debuglog.h 1835 2006-01-25 10:42:23Z rousseau $
 */

/**
 * @file
 * @brief This handles debugging.
 *
 * @note log message is sent to syslog or stderr depending on --foreground
 * command line argument
 *
 * @test
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

#ifndef __debug_h__
#define __debug_h__

#ifdef PCSC
/* use syslog, etc. if we are included from a file for pcscd */
#include "debuglog.h"
#else

#ifdef __cplusplus
extern "C"
{
#endif

enum {
	PCSC_LOG_DEBUG = 0,
	PCSC_LOG_INFO,
	PCSC_LOG_ERROR,
	PCSC_LOG_CRITICAL
};

#include <stdio.h>

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#define Log0(priority) log_msg(priority, "%s:%d:%s()", __FILE__, __LINE__, __FUNCTION__)
#define Log1(priority, fmt) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__)
#define Log2(priority, fmt, data) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define Log3(priority, fmt, data1, data2) log_msg(priority, "%s:%d:%s() " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define LogXxd(priority, msg, buffer, size) log_xxd(priority, msg, buffer, size)

void log_msg(const int priority, const char *fmt, ...);
void log_xxd(const int priority, const char *msg,
	const unsigned char *buffer, const int size);

#ifdef __cplusplus
}
#endif

#endif

#endif							/* __debug_h__ */

