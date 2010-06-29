/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/*
 * @file
 * @brief This abstracts dynamic library loading functions and timing.
 */

#include "config.h"
#include <string.h>
#ifdef HAVE_DL_H
#include <dl.h>
#include <errno.h>

#include "pcsclite.h"
#include "debuglog.h"
#include "dyn_generic.h"

int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{

	shl_t myHandle;

	*pvLHandle = 0;
	myHandle =
		shl_load(pcLibrary, BIND_IMMEDIATE | BIND_VERBOSE | BIND_NOSTART,
		0L);

	if (myHandle == 0)
	{
		Log3(PCSC_LOG_ERROR, "%s: %s", pcLibrary, strerror(errno));
		return SCARD_F_UNKNOWN_ERROR;
	}

	*pvLHandle = (void *) myHandle;
	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{

	int rv;

	rv = shl_unload((shl_t) * pvLHandle);
	*pvLHandle = 0;

	if (rv == -1)
	{
		Log2(PCSC_LOG_ERROR, "%s", strerror(errno));
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, const char *pcFunction)
{

	int rv;

	*pvFHandle = 0;
	rv = shl_findsym((shl_t *) & pvLHandle, pcFunction, TYPE_PROCEDURE,
		pvFHandle);

	if (rv == -1)
	{
		Log3(PCSC_LOG_ERROR, "%s: %s", pcFunction, strerror(errno));
		rv = SCARD_F_UNKNOWN_ERROR;
	}
	else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* HAVE_DL_H */

