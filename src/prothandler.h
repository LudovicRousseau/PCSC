/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles protocol defaults, PTS, etc.
 */

#ifndef __prothandler_h__
#define __prothandler_h__

	DWORD PHSetProtocol(struct ReaderContext *, DWORD, UCHAR, UCHAR);

#define SET_PROTOCOL_WRONG_ARGUMENT -1
#define SET_PROTOCOL_PPS_FAILED -2

#endif							/* __prothandler_h__ */
