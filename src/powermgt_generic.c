/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : powermgt_generic.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 09/07/03
	License: Copyright (C) 2000-2003 David Corcoran, <corcoran@linuxnet.com>

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

#if !defined(__APPLE__)

ULONG PMRegisterForPowerEvents()
{
  return 0;
}

#endif
