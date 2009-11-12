/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2003
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003
 *  Antti Tapaninen
 * Copyright (C) 2004-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles power management routines.
 */

#include "config.h"
#include "pcsclite.h"
#include "powermgt_generic.h"

/*
 * Check for platforms that have their own specific support.
 * It's more easy and flexible to do it here, rather than
 * with automake conditionals in src/Makefile.am.
 * No, it's still not a perfect solution design wise.
 */

ULONG PMRegisterForPowerEvents(void)
{
  return 0;
}
