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
 * @brief Fake PC/SC library (example code)
 */

/* define PCSC_API used in winscard.h */
#include "misc.h"
#include "winscard.h"

LONG SCardEstablishContext(DWORD dwScope,
	LPCVOID pvReserved1, LPCVOID pvReserved2,
	LPSCARDCONTEXT phContext)
{
	(void)dwScope;
	(void)pvReserved1;
	(void)pvReserved2;
	(void)phContext;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	(void)hContext;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardIsValidContext(SCARDCONTEXT hContext)
{
	(void)hContext;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardConnect(SCARDCONTEXT hContext,
	LPCSTR szReader,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	(void)hContext;
	(void)szReader;
	(void)dwShareMode;
	(void)dwPreferredProtocols;
	(void)phCard;
	(void)pdwActiveProtocol;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardReconnect(SCARDHANDLE hCard,
	DWORD dwShareMode,
	DWORD dwPreferredProtocols,
	DWORD dwInitialization, LPDWORD pdwActiveProtocol)
{
	(void)hCard;
	(void)dwShareMode;
	(void)dwPreferredProtocols;
	(void)dwInitialization;
	(void)pdwActiveProtocol;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	(void)hCard;
	(void)dwDisposition;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	(void)hCard;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	(void)hCard;
	(void)dwDisposition;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardStatus(SCARDHANDLE hCard,
	LPSTR mszReaderName,
	LPDWORD pcchReaderLen,
	LPDWORD pdwState,
	LPDWORD pdwProtocol,
	LPBYTE pbAtr,
	LPDWORD pcbAtrLen)
{
	(void)hCard;
	(void)mszReaderName;
	(void)pcchReaderLen;
	(void)pdwState;
	(void)pdwProtocol;
	(void)pbAtr;
	(void)pcbAtrLen;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext,
	DWORD dwTimeout,
	SCARD_READERSTATE *rgReaderStates, DWORD cReaders)
{
	(void)hContext;
	(void)dwTimeout;
	(void)rgReaderStates;
	(void)cReaders;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode,
	LPCVOID pbSendBuffer, DWORD cbSendLength,
	LPVOID pbRecvBuffer, DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	(void)hCard;
	(void)dwControlCode;
	(void)pbSendBuffer;
	(void)cbSendLength;
	(void)pbRecvBuffer;
	(void)cbRecvLength;
	(void)lpBytesReturned;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardTransmit(SCARDHANDLE hCard,
	const SCARD_IO_REQUEST *pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	SCARD_IO_REQUEST *pioRecvPci,
	LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	(void)hCard;
	(void)pioSendPci;
	(void)pbSendBuffer;
	(void)cbSendLength;
	(void)pioRecvPci;
	(void)pbRecvBuffer;
	(void)pcbRecvLength;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardListReaderGroups(SCARDCONTEXT hContext,
	LPSTR mszGroups, LPDWORD pcchGroups)
{
	(void)hContext;
	(void)mszGroups;
	(void)pcchGroups;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardListReaders(SCARDCONTEXT hContext,
	LPCSTR mszGroups,
	LPSTR mszReaders,
	LPDWORD pcchReaders)
{
	(void)hContext;
	(void)mszGroups;
	(void)mszReaders;
	(void)pcchReaders;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	(void)hContext;
	(void)pvMem;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{
	(void)hContext;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	(void)hCard;
	(void)dwAttrId;
	(void)pbAttr;
	(void)pcbAttrLen;

	return SCARD_F_INTERNAL_ERROR;
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPCBYTE pbAttr, DWORD cbAttrLen)
{
	(void)hCard;
	(void)dwAttrId;
	(void)pbAttr;
	(void)cbAttrLen;

	return SCARD_F_INTERNAL_ERROR;
}
