/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : hotplug.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 10/25/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides a search API for hot pluggble
	             devices.
	            
********************************************************************/

#ifndef __hotplug_h__
#define __hotplug_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG HPSearchHotPluggables();

#ifdef __cplusplus
}
#endif

#endif
