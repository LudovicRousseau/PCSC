/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : hotplug.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 10/25/00
	License: Copyright (C) 2000-2003 David Corcoran, <corcoran@linuxnet.com>

	Purpose: This provides a search API for hot pluggble  devices.

$Id$
********************************************************************/

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"

/*
 * Check for platforms that have their own specific support.
 * It's more easy and flexible to do it here, rather than
 * with automake conditionals in src/Makefile.am.
 * No, it's still not a perfect solution design wise.
 */

#if !defined(__APPLE__) && !defined(HAVE_LIBUSB) && !defined(__linux__)

LONG HPSearchHotPluggables(void)
{
	return 0;
}

ULONG HPRegisterForHotplugEvents(void)
{
	return 0;
}

#endif
