/*
 * This handles power management routines.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2002
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#ifndef __powermgt_generic_h__
#define __powermgt_generic_h__

#ifdef __cplusplus
extern "C"
{
#endif


/* 
 * Registers for Power Management callbacks 
 */

ULONG PMRegisterForPowerEvents();


#ifdef __cplusplus
}
#endif

#endif
