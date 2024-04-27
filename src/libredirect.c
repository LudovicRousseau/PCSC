/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2024
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

#define DLSYM_DECLARE(symbol)          \
		typeof(symbol)* symbol
#define DLSYM_SET_VALUE(symbol)        \
		.symbol = (typeof(symbol)(*))internal_error

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
	DLSYM_DECLARE(SCardEstablishContext);
	DLSYM_DECLARE(SCardReleaseContext);
	DLSYM_DECLARE(SCardIsValidContext);
	DLSYM_DECLARE(SCardConnect);
	DLSYM_DECLARE(SCardReconnect);
	DLSYM_DECLARE(SCardDisconnect);
	DLSYM_DECLARE(SCardBeginTransaction);
	DLSYM_DECLARE(SCardEndTransaction);
	DLSYM_DECLARE(SCardStatus);
	DLSYM_DECLARE(SCardGetStatusChange);
	DLSYM_DECLARE(SCardControl);
	DLSYM_DECLARE(SCardTransmit);
	DLSYM_DECLARE(SCardListReaderGroups);
	DLSYM_DECLARE(SCardListReaders);
	DLSYM_DECLARE(SCardFreeMemory);
	DLSYM_DECLARE(SCardCancel);
	DLSYM_DECLARE(SCardGetAttrib);
	DLSYM_DECLARE(SCardSetAttrib);
} redirect = {
	/* initialized with the fake internal_error() function */
	DLSYM_SET_VALUE(SCardEstablishContext),
	DLSYM_SET_VALUE(SCardReleaseContext),
	DLSYM_SET_VALUE(SCardIsValidContext),
	DLSYM_SET_VALUE(SCardConnect),
	DLSYM_SET_VALUE(SCardReconnect),
	DLSYM_SET_VALUE(SCardDisconnect),
	DLSYM_SET_VALUE(SCardBeginTransaction),
	DLSYM_SET_VALUE(SCardEndTransaction),
	DLSYM_SET_VALUE(SCardStatus),
	DLSYM_SET_VALUE(SCardGetStatusChange),
	DLSYM_SET_VALUE(SCardControl),
	DLSYM_SET_VALUE(SCardTransmit),
	DLSYM_SET_VALUE(SCardListReaderGroups),
	DLSYM_SET_VALUE(SCardListReaders),
	DLSYM_SET_VALUE(SCardFreeMemory),
	DLSYM_SET_VALUE(SCardCancel),
	DLSYM_SET_VALUE(SCardGetAttrib),
	DLSYM_SET_VALUE(SCardSetAttrib)
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
PCSC_API LONG SCardEstablishContext(DWORD dwScope,
	LPCVOID pvReserved1,
	LPCVOID pvReserved2,
	LPSCARDCONTEXT phContext)
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

PCSC_API LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	return redirect.SCardReleaseContext(hContext);
}

PCSC_API LONG SCardIsValidContext(SCARDCONTEXT hContext)
{
	return redirect.SCardIsValidContext(hContext);
}

PCSC_API LONG SCardConnect(SCARDCONTEXT hContext,
	LPCSTR szReader,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	return redirect.SCardConnect(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
}

PCSC_API LONG SCardReconnect(SCARDHANDLE hCard,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	return redirect.SCardReconnect(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
}

PCSC_API LONG SCardDisconnect(SCARDHANDLE hCard,
	DWORD dwDisposition)
{
	return redirect.SCardDisconnect(hCard, dwDisposition);
}

PCSC_API LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	return redirect.SCardBeginTransaction(hCard);
}

PCSC_API LONG SCardEndTransaction(SCARDHANDLE hCard,
	DWORD dwDisposition)
{
	return redirect.SCardEndTransaction(hCard, dwDisposition);
}

PCSC_API LONG SCardStatus(SCARDHANDLE hCard,
	LPSTR mszReaderName,
	LPDWORD pcchReaderLen,
	LPDWORD pdwState,
	LPDWORD pdwProtocol,
	LPBYTE pbAtr,
	LPDWORD pcbAtrLen)
{
	return redirect.SCardStatus(hCard, mszReaderName, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
}

PCSC_API LONG SCardGetStatusChange(SCARDCONTEXT hContext,
	DWORD dwTimeout,
	SCARD_READERSTATE *rgReaderStates,
	DWORD cReaders)
{
	return redirect.SCardGetStatusChange(hContext, dwTimeout, rgReaderStates,
		cReaders);
}

PCSC_API LONG SCardControl(SCARDHANDLE hCard,
	DWORD dwControlCode,
	LPCVOID pbSendBuffer,
	DWORD cbSendLength,
	LPVOID pbRecvBuffer,
	DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	return redirect.SCardControl(hCard, dwControlCode, pbSendBuffer, cbSendLength,
		pbRecvBuffer, cbRecvLength, lpBytesReturned);
}

PCSC_API LONG SCardTransmit(SCARDHANDLE hCard,
	const SCARD_IO_REQUEST *pioSendPci,
	LPCBYTE pbSendBuffer,
	DWORD cbSendLength,
	SCARD_IO_REQUEST *pioRecvPci,
	LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	return redirect.SCardTransmit(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
}

PCSC_API LONG SCardListReaderGroups(SCARDCONTEXT hContext,
	LPSTR mszGroups,
	LPDWORD pcchGroups)
{
	return redirect.SCardListReaderGroups(hContext, mszGroups, pcchGroups);
}

PCSC_API LONG SCardListReaders(SCARDCONTEXT hContext,
	LPCSTR mszGroups,
	LPSTR mszReaders,
	LPDWORD pcchReaders)
{
	return redirect.SCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
}

PCSC_API LONG SCardFreeMemory(SCARDCONTEXT hContext,
	LPCVOID pvMem)
{
	return redirect.SCardFreeMemory(hContext, pvMem);
}

PCSC_API LONG SCardCancel(SCARDCONTEXT hContext)
{
	return redirect.SCardCancel(hContext);
}

PCSC_API LONG SCardGetAttrib(SCARDHANDLE hCard,
	DWORD dwAttrId,
	LPBYTE pbAttr,
	LPDWORD pcbAttrLen)
{
	return redirect.SCardGetAttrib(hCard, dwAttrId, pbAttr, pcbAttrLen);
}

PCSC_API LONG SCardSetAttrib(SCARDHANDLE hCard,
	DWORD dwAttrId,
	LPCBYTE pbAttr,
	DWORD cbAttrLen)
{
	return redirect.SCardSetAttrib(hCard, dwAttrId, pbAttr, cbAttrLen);
}

/** Protocol Control Information for T=0 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for T=1 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for raw access */
PCSC_API const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, sizeof(SCARD_IO_REQUEST) };

