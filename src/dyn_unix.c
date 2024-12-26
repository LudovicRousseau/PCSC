/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2024
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This abstracts dynamic library loading functions and timing.
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#ifndef __APPLE__
#include <dlfcn.h>
#include <stdlib.h>
#include <stdbool.h>

#include "misc.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "dyn_generic.h"

INTERNAL void * DYN_LoadLibrary(const char *pcLibrary)
{
	void *pvLHandle = NULL;
#ifndef PCSCLITE_STATIC_DRIVER
	pvLHandle = dlopen(pcLibrary, RTLD_LAZY);

	if (pvLHandle == NULL)
	{
		Log3(PCSC_LOG_CRITICAL, "%s: %s", pcLibrary, dlerror());
	}
#else
	(void)pcLibrary;
#endif

	return pvLHandle;
}

INTERNAL LONG DYN_CloseLibrary(void *pvLHandle)
{
#ifndef PCSCLITE_STATIC_DRIVER
	int ret;

	ret = dlclose(pvLHandle);

	if (ret)
	{
		Log2(PCSC_LOG_CRITICAL, "%s", dlerror());
		return SCARD_F_UNKNOWN_ERROR;
	}
#else
	(void)pvLHandle;
#endif

	return SCARD_S_SUCCESS;
}

INTERNAL LONG DYN_GetAddress(void *pvLHandle, void **pvFHandle,
	const char *pcFunction, bool mayfail)
{
	char pcFunctionName[256];
	LONG rv = SCARD_S_SUCCESS;

	/* Some platforms might need a leading underscore for the symbol */
	(void)snprintf(pcFunctionName, sizeof(pcFunctionName), "_%s", pcFunction);

	*pvFHandle = NULL;
#ifndef PCSCLITE_STATIC_DRIVER
	*pvFHandle = dlsym(pvLHandle, pcFunctionName);

	/* Failed? Try again without the leading underscore */
	if (*pvFHandle == NULL)
		*pvFHandle = dlsym(pvLHandle, pcFunction);

	if (*pvFHandle == NULL)
	{
#ifdef NO_LOG
		(void)mayfail;
#endif
		Log3(mayfail ? PCSC_LOG_INFO : PCSC_LOG_CRITICAL, "%s: %s",
			pcFunction, dlerror());
		rv = SCARD_F_UNKNOWN_ERROR;
	}
#else
	(void)pvLHandle;
	(void)pvFHandle;
	(void)mayfail;
#endif

	return rv;
}

#endif	/* !__APPLE__ */
