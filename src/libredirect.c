/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2025
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
 * @brief Redirect PC/SC calls to the delegate library
 */

#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include "misc.h"
#include <winscard.h>
#include "sys_generic.h"

#define DEBUG

/* function prototypes */

#define p_SCardEstablishContext(fct) LONG(fct)(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)

#define p_SCardReleaseContext(fct) LONG(fct)(SCARDCONTEXT hContext)

#define p_SCardIsValidContext(fct) LONG(fct) (SCARDCONTEXT hContext)

#define p_SCardConnect(fct) LONG(fct) (SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)

#define p_SCardReconnect(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwShareMode, DWORD dwPreferredProtocols, DWORD dwInitialization, LPDWORD pdwActiveProtocol)

#define p_SCardDisconnect(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwDisposition)

#define p_SCardBeginTransaction(fct) LONG(fct) (SCARDHANDLE hCard)

#define p_SCardEndTransaction(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwDisposition)

#define p_SCardStatus(fct) LONG(fct) (SCARDHANDLE hCard, LPSTR mszReaderName, LPDWORD pcchReaderLen, LPDWORD pdwState, LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)

#define p_SCardGetStatusChange(fct) LONG(fct) (SCARDCONTEXT hContext, DWORD dwTimeout, LPSCARD_READERSTATE rgReaderStates, DWORD cReaders)

#define p_SCardControl(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer, DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned)

#define p_SCardTransmit(fct) LONG(fct) (SCARDHANDLE hCard, const SCARD_IO_REQUEST * pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength, SCARD_IO_REQUEST * pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)

#define p_SCardListReaderGroups(fct) LONG(fct) (SCARDCONTEXT hContext, LPSTR mszGroups, LPDWORD pcchGroups)

#define p_SCardListReaders(fct) LONG(fct) (SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)

#define p_SCardFreeMemory(fct) LONG(fct) (SCARDCONTEXT hContext, LPCVOID pvMem)

#define p_SCardCancel(fct) LONG(fct) (SCARDCONTEXT hContext)

#define p_SCardGetAttrib(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr, LPDWORD pcbAttrLen)

#define p_SCardSetAttrib(fct) LONG(fct) (SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr, DWORD cbAttrLen)

/* fake function to just return en error code */
static LONG internal_error(void)
{
	return SCARD_F_INTERNAL_ERROR;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
/* contains pointers to real functions */
static struct
{
	p_SCardEstablishContext(*SCardEstablishContext);
	p_SCardReleaseContext(*SCardReleaseContext);
	p_SCardIsValidContext(*SCardIsValidContext);
	p_SCardConnect(*SCardConnect);
	p_SCardReconnect(*SCardReconnect);
	p_SCardDisconnect(*SCardDisconnect);
	p_SCardBeginTransaction(*SCardBeginTransaction);
	p_SCardEndTransaction(*SCardEndTransaction);
	p_SCardStatus(*SCardStatus);
	p_SCardGetStatusChange(*SCardGetStatusChange);
	p_SCardControl(*SCardControl);
	p_SCardTransmit(*SCardTransmit);
	p_SCardListReaderGroups(*SCardListReaderGroups);
	p_SCardListReaders(*SCardListReaders);
	p_SCardFreeMemory(*SCardFreeMemory);
	p_SCardCancel(*SCardCancel);
	p_SCardGetAttrib(*SCardGetAttrib);
	p_SCardSetAttrib(*SCardSetAttrib);
} redirect = {
	/* initialized with the fake internal_error() function */
	.SCardEstablishContext = (p_SCardEstablishContext(*))internal_error,
	.SCardReleaseContext = (p_SCardReleaseContext(*))internal_error,
	.SCardIsValidContext = (p_SCardIsValidContext(*))internal_error,
	.SCardConnect = (p_SCardConnect(*))internal_error,
	.SCardReconnect = (p_SCardReconnect(*))internal_error,
	.SCardDisconnect = (p_SCardDisconnect(*))internal_error,
	.SCardBeginTransaction = (p_SCardBeginTransaction(*))internal_error,
	.SCardEndTransaction = (p_SCardEndTransaction(*))internal_error,
	.SCardStatus = (p_SCardStatus(*))internal_error,
	.SCardGetStatusChange = (p_SCardGetStatusChange(*))internal_error,
	.SCardControl = (p_SCardControl(*))internal_error,
	.SCardTransmit = (p_SCardTransmit(*))internal_error,
	.SCardListReaderGroups = (p_SCardListReaderGroups(*))internal_error,
	.SCardListReaders = (p_SCardListReaders(*))internal_error,
	.SCardFreeMemory = (p_SCardFreeMemory(*))internal_error,
	.SCardCancel = (p_SCardCancel(*))internal_error,
	.SCardGetAttrib = (p_SCardGetAttrib(*))internal_error,
	.SCardSetAttrib = (p_SCardSetAttrib(*))internal_error,
};
#pragma GCC diagnostic pop

static void *Lib_handle = NULL;

#ifdef DEBUG
static void log_line(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	printf("\n");
	va_end(args);
}
#else
static void log_line(const char *fmt, ...)
{
}
#endif

static LONG load_lib(void)
{
#define LIBPCSC "libpcsclite_real.so.1"

	const char *lib;

	lib = SYS_GetEnv("LIBPCSCLITE_DELEGATE");
	if (NULL == lib)
		lib = LIBPCSC;

	/* load the real library */
	Lib_handle = dlopen(lib, RTLD_LAZY);
	if (NULL == Lib_handle)
	{
		log_line("loading \"%s\" failed: %s", lib, dlerror());
		return SCARD_F_INTERNAL_ERROR;
	}

#define get_symbol(s) do { redirect.s = dlsym(Lib_handle, #s); if (NULL == redirect.s) { log_line("%s", dlerror()); return SCARD_F_INTERNAL_ERROR; } } while (0)

	if (SCardEstablishContext == dlsym(Lib_handle, "SCardEstablishContext"))
	{
		log_line("Symbols dlsym error");
		return SCARD_F_INTERNAL_ERROR;
	}

	get_symbol(SCardEstablishContext);
	get_symbol(SCardReleaseContext);
	get_symbol(SCardIsValidContext);
	get_symbol(SCardConnect);
	get_symbol(SCardReconnect);
	get_symbol(SCardDisconnect);
	get_symbol(SCardBeginTransaction);
	get_symbol(SCardEndTransaction);
	get_symbol(SCardStatus);
	get_symbol(SCardGetStatusChange);
	get_symbol(SCardControl);
	get_symbol(SCardTransmit);
	get_symbol(SCardListReaderGroups);
	get_symbol(SCardListReaders);
	get_symbol(SCardFreeMemory);
	get_symbol(SCardCancel);
	get_symbol(SCardGetAttrib);
	get_symbol(SCardSetAttrib);

	return SCARD_S_SUCCESS;
}


/* exported functions */
PCSC_API p_SCardEstablishContext(SCardEstablishContext)
{
	LONG rv;
	static int init = 0;

	if (!init)
	{
		init = 1;

		/* load the real library */
		rv = load_lib();
		if (rv != SCARD_S_SUCCESS)
			return rv;
	}

	return redirect.SCardEstablishContext(dwScope, pvReserved1, pvReserved2,
		phContext);
	return rv;
}

PCSC_API p_SCardReleaseContext(SCardReleaseContext)
{
	return redirect.SCardReleaseContext(hContext);
}

PCSC_API p_SCardIsValidContext(SCardIsValidContext)
{
	return redirect.SCardIsValidContext(hContext);
}

PCSC_API p_SCardConnect(SCardConnect)
{
	return redirect.SCardConnect(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
}

PCSC_API p_SCardReconnect(SCardReconnect)
{
	return redirect.SCardReconnect(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
}

PCSC_API p_SCardDisconnect(SCardDisconnect)
{
	return redirect.SCardDisconnect(hCard, dwDisposition);
}

PCSC_API p_SCardBeginTransaction(SCardBeginTransaction)
{
	return redirect.SCardBeginTransaction(hCard);
}

PCSC_API p_SCardEndTransaction(SCardEndTransaction)
{
	return redirect.SCardEndTransaction(hCard, dwDisposition);
}

PCSC_API p_SCardStatus(SCardStatus)
{
	return redirect.SCardStatus(hCard, mszReaderName, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
}

PCSC_API p_SCardGetStatusChange(SCardGetStatusChange)
{
	return redirect.SCardGetStatusChange(hContext, dwTimeout, rgReaderStates,
		cReaders);
}

PCSC_API p_SCardControl(SCardControl)
{
	return redirect.SCardControl(hCard, dwControlCode, pbSendBuffer, cbSendLength,
		pbRecvBuffer, cbRecvLength, lpBytesReturned);
}

PCSC_API p_SCardTransmit(SCardTransmit)
{
	return redirect.SCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
}

PCSC_API p_SCardListReaderGroups(SCardListReaderGroups)
{
	return redirect.SCardListReaderGroups(hContext, mszGroups, pcchGroups);
}

PCSC_API p_SCardListReaders(SCardListReaders)
{
	return redirect.SCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
}

PCSC_API p_SCardFreeMemory(SCardFreeMemory)
{
	return redirect.SCardFreeMemory(hContext, pvMem);
}

PCSC_API p_SCardCancel(SCardCancel)
{
	return redirect.SCardCancel(hContext);
}

PCSC_API p_SCardGetAttrib(SCardGetAttrib)
{
	return redirect.SCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
}

PCSC_API p_SCardSetAttrib(SCardSetAttrib)
{
	return redirect.SCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
}

/** Protocol Control Information for T=0 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for T=1 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for raw access */
PCSC_API const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, sizeof(SCARD_IO_REQUEST) };

