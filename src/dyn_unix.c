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
#include <string.h>
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
	int rv, iSize;
	char *pcFunctionName;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	pcFunctionName = NULL;
	iSize = 0;

	iSize = strlen(pcFunction) + 2;	/* 1-NULL, 2-_ */
	pcFunctionName = (char *) malloc(iSize * sizeof(char));
	pcFunctionName[0] = '_';
	strcpy(&pcFunctionName[1], pcFunction);

	*pvFHandle = NULL;
	*pvFHandle = dlsym(pvLHandle, pcFunctionName);

	/*
	 * also try without a leading '_' (needed for FreeBSD) 
	 */
	if (*pvFHandle == NULL)
		*pvFHandle = dlsym(pvLHandle, pcFunction);

	if (*pvFHandle == NULL)
	{
		DebugLogB("DYN_GetAddress: dlerror() reports %s", dlerror());
		rv = SCARD_F_UNKNOWN_ERROR;
	} else
		rv = SCARD_S_SUCCESS;

	free(pcFunctionName);

	return rv;
}
