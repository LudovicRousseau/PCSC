/*
 * This handles smartcard reader communications.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#ifndef __winscard_h__
#define __winscard_h__

#if defined(__APPLE__)
#include <PCSC/pcsclite.h>
#else
#include <pcsclite.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	LONG SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);

	LONG SCardReleaseContext(SCARDCONTEXT hContext);

	LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout);

	LONG SCardConnect(SCARDCONTEXT hContext,
		LPCSTR szReader,
		DWORD dwShareMode,
		DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

	LONG SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode,
		DWORD dwPreferredProtocols,
		DWORD dwInitialization, LPDWORD pdwActiveProtocol);

	LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition);

	LONG SCardBeginTransaction(SCARDHANDLE hCard);

	LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition);

	LONG SCardCancelTransaction(SCARDHANDLE hCard);

	LONG SCardStatus(SCARDHANDLE hCard,
		LPSTR mszReaderNames, LPDWORD pcchReaderLen,
		LPDWORD pdwState,
		LPDWORD pdwProtocol,
		LPBYTE pbAtr, LPDWORD pcbAtrLen);

	LONG SCardGetStatusChange(SCARDCONTEXT hContext,
		DWORD dwTimeout,
		LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders);

	LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode,
		LPCVOID pbSendBuffer, DWORD cbSendLength,
		LPVOID pbRecvBuffer, DWORD cbRecvLength, DWORD *lpBytesReturned);

	LONG SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci,
		LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci,
		LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength);

	LONG SCardListReaderGroups(SCARDCONTEXT hContext,
		LPSTR mszGroups, LPDWORD pcchGroups);

	LONG SCardListReaders(SCARDCONTEXT hContext,
		LPCSTR mszGroups,
		LPSTR mszReaders, LPDWORD pcchReaders);

	LONG SCardCancel(SCARDCONTEXT hContext);

	void SCardUnload(void);

#ifdef __cplusplus
}
#endif

#endif

