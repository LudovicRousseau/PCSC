/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
 	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
		     Copyright (C) 2003, Ludovic Rousseau
		     <ludovic.rousseau@free.fr>
            Purpose: This handles smartcard reader communications. 
	            
$Id$

********************************************************************/

#ifndef __winscard_h__
#define __winscard_h__

//#ifndef __APPLE__
#include <pcsclite.h>
//#else
//#include <PCSC/pcsclite.h>
//#endif

#ifdef __cplusplus
extern "C"
{
#endif

	long SCardEstablishContext(unsigned long dwScope,
		const void *pvReserved1, const void *pvReserved2, long *phContext);

	long SCardReleaseContext(long hContext);

	long SCardSetTimeout(long hContext, unsigned long dwTimeout);

	long SCardConnect(long hContext,
		const char *szReader,
		unsigned long dwShareMode,
		unsigned long dwPreferredProtocols,
		long *phCard, unsigned long *pdwActiveProtocol);

	long SCardReconnect(long hCard,
		unsigned long dwShareMode,
		unsigned long dwPreferredProtocols,
		unsigned long dwInitialization, unsigned long *pdwActiveProtocol);

	long SCardDisconnect(long hCard, unsigned long dwDisposition);

	long SCardBeginTransaction(long hCard);

	long SCardEndTransaction(long hCard, unsigned long dwDisposition);

	long SCardCancelTransaction(long hCard);

	long SCardStatus(long hCard,
		char *mszReaderNames,
		unsigned long *pcchReaderLen,
		unsigned long *pdwState,
		unsigned long *pdwProtocol,
		unsigned char *pbAtr, unsigned long *pcbAtrLen);

	long SCardGetStatusChange(long hContext,
		unsigned long dwTimeout,
		LPSCARD_READERSTATE_A rgReaderStates, unsigned long cReaders);

	long SCardControl(long hCard,
		const unsigned char *pbSendBuffer,
		unsigned long cbSendLength,
		unsigned char *pbRecvBuffer, unsigned long *pcbRecvLength);

	long SCardTransmit(long hCard,
		LPCSCARD_IO_REQUEST pioSendPci,
		const unsigned char *pbSendBuffer,
		unsigned long cbSendLength,
		LPSCARD_IO_REQUEST pioRecvPci,
		unsigned char *pbRecvBuffer, unsigned long *pcbRecvLength);

	long SCardListReaderGroups(long hContext,
		char *mszGroups, unsigned long *pcchGroups);

	long SCardListReaders(long hContext,
		const char *mszGroups,
		char *mszReaders, unsigned long *pcchReaders);

	long SCardCancel(long hContext);

	void SCardUnload(void);

#ifdef __cplusplus
}
#endif

#endif

