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

#ifndef __hotplug_h__
#define __hotplug_h__

#ifdef __cplusplus
extern "C"
{
#endif
	LONG HPSearchHotPluggables(void);
	ULONG HPRegisterForHotplugEvents(void);
#ifdef __cplusplus
}
#endif

#endif

