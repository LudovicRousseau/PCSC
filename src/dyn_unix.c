/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : dyn_unix.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 8/12/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This abstracts dynamic library loading 
                     functions and timing. 

********************************************************************/

#include <string.h>
#include <dlfcn.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "dyn_generic.h"
#include "debuglog.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{

	const char *error;

	/*
	 * Zero out everything 
	 */
	error = NULL;

	*pvLHandle = 0;
	*pvLHandle = dlopen(pcLibrary, RTLD_LAZY);

	if ((error = dlerror()) != NULL)
	{
		DebugLogB("DYN_LoadLibrary: dlerror() reports %s", error);
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{

	const char *error;

	/*
	 * Zero out everything 
	 */
	error = NULL;

	dlclose(*pvLHandle);
	*pvLHandle = 0;

	if ((error = dlerror()) != NULL)
	{
		DebugLogB("DYN_CloseLibrary: dlerror() reports %s", error);
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, char *pcFunction)
{

	int rv;
	const char *error;
	char *pcFunctionName;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	error = NULL;
	pcFunctionName = 0;

	pcFunctionName = pcFunction;

	*pvFHandle = 0;
	*pvFHandle = dlsym(pvLHandle, pcFunctionName);

	if ((error = dlerror()) != NULL)
	{
		DebugLogB("DYN_GetAddress: dlerror() reports %s", error);
		rv = SCARD_F_UNKNOWN_ERROR;
	} else
	{
		rv = SCARD_S_SUCCESS;
	}

	return rv;
}
