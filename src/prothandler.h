/*
 * This handles protocol defaults, PTS, etc.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#ifndef __prothandler_h__
#define __prothandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

	UCHAR PHGetDefaultProtocol(PUCHAR, DWORD);
	UCHAR PHGetAvailableProtocols(PUCHAR, DWORD);
	DWORD PHSetProtocol(struct ReaderContext *, DWORD, UCHAR, UCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __prothandler_h__ */
