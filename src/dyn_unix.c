/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : dyn_unix.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 8/12/99
	License: Copyright (C) 1999 David Corcoran <corcoran@linuxnet.com>
	Purpose: This abstracts dynamic library loading functions and timing. 

$Id$

********************************************************************/

#include "config.h"
#include <stdio.h>
#include <string.h>
#if defined(HAVE_DLFCN_H) && !defined(HAVE_DL_H) && !defined(__APPLE__)
#include <dlfcn.h>
#include <stdlib.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "dyn_generic.h"
#include "debuglog.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{
	*pvLHandle = NULL;
	*pvLHandle = dlopen(pcLibrary, RTLD_LAZY);

	if (*pvLHandle == NULL)
	{
		DebugLogB("DYN_LoadLibrary: dlerror() reports %s", dlerror());
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{
	int ret;

	ret = dlclose(*pvLHandle);
	*pvLHandle = NULL;

	if (ret)
	{
		DebugLogB("DYN_CloseLibrary: dlerror() reports %s", dlerror());
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, char *pcFunction)
{
	char pcFunctionName[256];
	int rv;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/* Some platforms might need a leading underscore for the symbol */
	snprintf(pcFunctionName, sizeof(pcFunctionName), "_%s", pcFunction);

	*pvFHandle = NULL;
	*pvFHandle = dlsym(pvLHandle, pcFunctionName);

	/* Failed? Try again without the leading underscore */
	if (*pvFHandle == NULL)
		*pvFHandle = dlsym(pvLHandle, pcFunction);

	if (*pvFHandle == NULL)
	{
		DebugLogB("DYN_GetAddress: dlerror() reports %s", dlerror());
		rv = SCARD_F_UNKNOWN_ERROR;
	} else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* HAVE_DLFCN_H && !HAVE_DL_H && !__APPLE__ */
