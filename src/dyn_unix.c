/*
 * This abstracts dynamic library loading functions and timing.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#if defined(HAVE_DLFCN_H) && !defined(HAVE_DL_H) && !defined(__APPLE__)
#include <dlfcn.h>
#include <stdlib.h>

#include "pcsclite.h"
#include "debuglog.h"
#include "dyn_generic.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{
	*pvLHandle = NULL;
	*pvLHandle = dlopen(pcLibrary, RTLD_LAZY);

	if (*pvLHandle == NULL)
	{
		Log3(PCSC_LOG_CRITICAL, "%s: %s", pcLibrary, dlerror());
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
		Log2(PCSC_LOG_CRITICAL, "%s", dlerror());
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, char *pcFunction)
{
	char pcFunctionName[256];
	int rv;

	/* Some platforms might need a leading underscore for the symbol */
	snprintf(pcFunctionName, sizeof(pcFunctionName), "_%s", pcFunction);

	*pvFHandle = NULL;
	*pvFHandle = dlsym(pvLHandle, pcFunctionName);

	/* Failed? Try again without the leading underscore */
	if (*pvFHandle == NULL)
		*pvFHandle = dlsym(pvLHandle, pcFunction);

	if (*pvFHandle == NULL)
	{
		Log3(PCSC_LOG_CRITICAL, "%s: %s", pcFunction, dlerror());
		rv = SCARD_F_UNKNOWN_ERROR;
	} else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* HAVE_DLFCN_H && !HAVE_DL_H && !__APPLE__ */
