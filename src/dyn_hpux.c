/*
 * This abstracts dynamic library loading functions and timing.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#include "config.h"
#include <string.h>
#ifdef HAVE_DL_H
#include <dl.h>
#include <errno.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "dyn_generic.h"
#include "debuglog.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{

	shl_t myHandle;

	*pvLHandle = 0;
	myHandle =
		shl_load(pcLibrary, BIND_IMMEDIATE | BIND_VERBOSE | BIND_NOSTART,
		0L);

	if (myHandle == 0)
	{
		DebugLogB("DYN_GetAddress: strerror() reports %s",
			strerror(errno));
		return SCARD_F_UNKNOWN_ERROR;
	}

	*pvLHandle = (void *) myHandle;
	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{

	int rv;
	/*
	 * Zero out everything 
	 */
	rv = 0;

	rv = shl_unload((shl_t) * pvLHandle);
	*pvLHandle = 0;

	if (rv == -1)
	{
		DebugLogB("DYN_GetAddress: strerror() reports %s",
			strerror(errno));
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
	rv = shl_findsym((shl_t *) & pvLHandle, pcFunction, TYPE_PROCEDURE,
		pvFHandle);

	if (rv == -1)
	{
		DebugLogB("DYN_GetAddress: strerror() reports %s",
			strerror(errno));
		rv = SCARD_F_UNKNOWN_ERROR;
	} else
	{
		rv = SCARD_S_SUCCESS;
	}

	return rv;
}

#endif	/* HAVE_DL_H */
