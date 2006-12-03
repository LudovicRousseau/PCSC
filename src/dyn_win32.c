/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

/**
 * @file
 * @brief This abstracts dynamic library loading functions and timing.
 */

#include "config.h"
#ifdef WIN32
#include <string.h>

#include "windows.h"
#include <winscard.h>
#include "dyn_generic.h"
#include "debug.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{
	*pvLHandle = NULL;
	*pvLHandle = LoadLibrary(pcLibrary);

	if (*pvLHandle == NULL)
	{
#if 0
		Log2(PCSC_LOG_ERROR, "DYN_LoadLibrary: dlerror() reports %s", dlerror());
#endif
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{
	int ret;

	ret = FreeLibrary(*pvLHandle);
	*pvLHandle = NULL;

	/* If the function fails, the return value is zero. To get extended error
	 * information, call GetLastError. */
	if (ret == 0)
	{
#if 0
		Log2(PCSC_LOG_ERROR, "DYN_CloseLibrary: dlerror() reports %s", dlerror());
#endif
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, const char *pcFunction)
{
	int rv;
	char *pcFunctionName;

	/*
	 * Zero out everything
	 */
	rv = 0;
	pcFunctionName = NULL;

	pcFunctionName = pcFunction;

	*pvFHandle = NULL;
	*pvFHandle = GetProcAddress(pvLHandle, pcFunctionName);

	if (*pvFHandle == NULL)
	{
#if 0
		Log2(PCSC_LOG_ERROR, "DYN_GetAddress: dlerror() reports %s", dlerror());
#endif
		rv = SCARD_F_UNKNOWN_ERROR;
	}
	else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* WIN32 */

