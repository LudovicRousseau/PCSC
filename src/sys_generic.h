/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles abstract system level calls.
 */

#ifndef __sys_generic_h__
#define __sys_generic_h__

#include <sys/stat.h>
#include <sys/mman.h>

	int SYS_Sleep(int);

	int SYS_USleep(int);

	int SYS_Daemon(int, int);

	int SYS_RandomInt(int, int);

	int SYS_GetSeed(void);

#endif							/* __sys_generic_h__ */
