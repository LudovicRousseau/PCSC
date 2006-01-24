/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles smartcard reader communications.
 */

#ifndef __winscard_h__
#define __winscard_h__

#include <pcsclite.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef PCSC_API
#define PCSC_API
#endif

	PCSC_API LONG SCardEstablishContext(DWORD dwScope,
		LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext);

	PCSC_API LONG SCardReleaseContext(SCARDCONTEXT hContext);

	PCSC_API LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout);

	PCSC_API LONG SCardConnect(SCARDCONTEXT hContext,
		LPCTSTR szReader,
		DWORD dwShareMode,
		DWORD dwPreferredProtocols,
		LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol);

	PCSC_API LONG SCardReconnect(SCARDHANDLE hCard,
		DWORD dwShareMode,
		DWORD dwPreferredProtocols,
		DWORD dwInitialization, LPDWORD pdwActiveProtocol);

	PCSC_API LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition);

	PCSC_API LONG SCardBeginTransaction(SCARDHANDLE hCard);

	PCSC_API LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition);

	PCSC_API LONG SCardCancelTransaction(SCARDHANDLE hCard);

	PCSC_API LONG SCardStatus(SCARDHANDLE hCard,
		LPTSTR mszReaderNames, LPDWORD pcchReaderLen,
		LPDWORD pdwState,
		LPDWORD pdwProtocol,
		LPBYTE pbAtr, LPDWORD pcbAtrLen);

	PCSC_API LONG SCardGetStatusChange(SCARDCONTEXT hContext,
		DWORD dwTimeout,
		LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders);

	PCSC_API LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode,
		LPCVOID pbSendBuffer, DWORD cbSendLength,
		LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned);

	PCSC_API LONG SCardTransmit(SCARDHANDLE hCard,
		LPCSCARD_IO_REQUEST pioSendPci,
		LPCBYTE pbSendBuffer, DWORD cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci,
		LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength);

	PCSC_API LONG SCardListReaderGroups(SCARDCONTEXT hContext,
		LPTSTR mszGroups, LPDWORD pcchGroups);

	PCSC_API LONG SCardListReaders(SCARDCONTEXT hContext,
		LPCTSTR mszGroups,
		LPTSTR mszReaders, LPDWORD pcchReaders);

	PCSC_API LONG SCardCancel(SCARDCONTEXT hContext);

	PCSC_API LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
		LPBYTE pbAttr, LPDWORD pcbAttrLen);

	PCSC_API LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
		LPCBYTE pbAttr, DWORD cbAttrLen);

	void SCardUnload(void);

#ifdef __cplusplus
}
#endif

#endif

