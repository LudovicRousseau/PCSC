/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This abstracts dynamic library loading functions.
 */

#ifndef __dyn_generic_h__
#define __dyn_generic_h__

	int DYN_LoadLibrary(void **, char *);
	int DYN_CloseLibrary(void **);
	int DYN_GetAddress(void *, /*@out@*/ void **, const char *);

#endif
