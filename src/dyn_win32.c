/*
 * This abstracts dynamic library loading functions and timing.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#include "config.h"
#ifdef WIN32
#include <string.h>

#include "windows.h"
#include <winscard.h>
#include "dyn_generic.h"
#include "debuglog.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{
	*pvLHandle = 0;
	*pvLHandle = LoadLibrary(pcLibrary);

	if (*pvLHandle == NULL)
	{
#if 0
		DebugLogB("DYN_LoadLibrary: dlerror() reports %s", dlerror());
#endif
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{
	int ret;

	ret = FreeLibrary(*pvLHandle);
	*pvLHandle = 0;

	if (ret)
	{
#if 0
		DebugLogB("DYN_CloseLibrary: dlerror() reports %s", dlerror());
#endif
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, char *pcFunction)
{
	int rv;
	char *pcFunctionName;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	pcFunctionName = 0;

	pcFunctionName = pcFunction;

	*pvFHandle = 0;
	*pvFHandle = GetProcAddress(pvLHandle, pcFunctionName);

	if (*pvFHandle == NULL)
	{
#if 0
		DebugLogB("DYN_GetAddress: dlerror() reports %s", dlerror());
#endif
		rv = SCARD_F_UNKNOWN_ERROR;
	}
	else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* WIN32 */
