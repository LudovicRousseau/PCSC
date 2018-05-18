/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2001
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2010
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

LONG DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
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

LONG DYN_CloseLibrary(void **pvLHandle)
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

LONG DYN_GetAddress(void *pvLHandle, void **pvFHandle, const char *pcFunction,
	int mayfail)
{

	int rv;

	*pvFHandle = 0;
	rv = shl_findsym((shl_t *) & pvLHandle, pcFunction, TYPE_PROCEDURE,
		pvFHandle);

	if (rv == -1)
	{
		Log3(mayfail ? PCSC_LOG_INFO : PCSC_LOG_ERROR, "%s: %s",
			pcFunction, strerror(errno));
		rv = SCARD_F_UNKNOWN_ERROR;
	}
	else
		rv = SCARD_S_SUCCESS;

	return rv;
}

#endif	/* HAVE_DL_H */

