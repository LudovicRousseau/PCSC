/*
 * This provides a search API for hot pluggble devices.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#include "config.h"
#include "PCSC/pcsclite.h"

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

LONG HPStopHotPluggables(void)
{
	return 0;
}

#endif
