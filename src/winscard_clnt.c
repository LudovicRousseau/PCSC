/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard_clnt.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 10/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles smartcard reader communications. 
	             This file forwards requests over message queues.

********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "debuglog.h"

#ifdef USE_THREAD_SAFETY
#include "thread_generic.h"
#endif

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"

#include "winscard_msg.h"

static struct _psChannelMap
{
	SCARDHANDLE hCard;
	LPSTR readerName;
}
psChannelMap[PCSCLITE_MAX_CONTEXTS];

static struct _psContextMap
{
	SCARDCONTEXT hContext;
	DWORD contextBlockStatus;
}
psContextMap[PCSCLITE_MAX_CONTEXTS];

static short isExecuted = 0;
static int parentPID = 0;
static int mapAddr = 0;

#ifdef USE_THREAD_SAFETY
static PCSCLITE_MUTEX clientMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static PREADER_STATES readerStates[PCSCLITE_MAX_CONTEXTS];

SCARD_IO_REQUEST g_rgSCardT0Pci, g_rgSCardT1Pci, g_rgSCardRawPci;

static LONG SCardAddContext(SCARDCONTEXT);
static LONG SCardGetContextIndice(SCARDCONTEXT);
static LONG SCardRemoveContext(SCARDCONTEXT);

static LONG SCardAddHandle(SCARDHANDLE, LPSTR);
static LONG SCardGetHandleIndice(SCARDHANDLE);
static LONG SCardRemoveHandle(SCARDHANDLE);

static LONG SCardCheckDaemonAvailability();
static LONG SCardCheckReaderAvailability(LPSTR, LONG);

/*
 * Thread safety functions 
 */
// static LONG SCardSetupThreadSafety ( );
static LONG SCardLockThread();
static LONG SCardUnlockThread();

static void SCardCleanupClient();

/*
 * by najam 
 */
static LONG SCardEstablishContextTH(DWORD, LPCVOID, LPCVOID,
	LPSCARDCONTEXT);
static LONG SCardReleaseContextTH(SCARDCONTEXT hContext);
static LONG SCardConnectTH(SCARDCONTEXT, LPCSTR, DWORD, DWORD,
	LPSCARDHANDLE, LPDWORD);
static LONG SCardReconnectTH(SCARDHANDLE, DWORD, DWORD, DWORD, LPDWORD);
static LONG SCardDisconnectTH(SCARDHANDLE, DWORD);
static LONG SCardEndTransactionTH(SCARDHANDLE, DWORD);
static LONG SCardCancelTransactionTH(SCARDHANDLE);
static LONG SCardStatusTH(SCARDHANDLE, LPSTR, LPDWORD, LPDWORD, LPDWORD,
	LPBYTE, LPDWORD);
static LONG SCardTransmitTH(SCARDHANDLE, LPCSCARD_IO_REQUEST, LPCBYTE,
	DWORD, LPSCARD_IO_REQUEST, LPBYTE, LPDWORD);
static LONG SCardListReadersTH(SCARDCONTEXT, LPCSTR, LPSTR, LPDWORD);
static LONG SCardListReaderGroupsTH(SCARDCONTEXT, LPSTR, LPDWORD);
static LONG SCardCancelTH(SCARDCONTEXT);

/*
 * -------by najam-------------------------------------- 
 */

/*
 * By najam 
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	long rv;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

	return rv;

}

/*
 * -----------by najam 
 */

static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{

	LONG liIndex, rv;
	int i, pageSize;
	establish_struct scEstablishStruct;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;
	i = 0;
	pageSize = 0;

	if (phContext == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	} else
	{
		*phContext = 0;
	}

	/*
	 * Do this only once 
	 */
	if (isExecuted == 0)
	{
		g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
		g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;
		g_rgSCardRawPci.dwProtocol = SCARD_PROTOCOL_RAW;

		/*
		 * Do any system initilization here 
		 */
		SYS_Initialize();

		/*
		 * Set up the parent's process ID 
		 */
		parentPID = SYS_GetPID();

		/*
		 * Set up the memory mapped reader stats structures 
		 */
		mapAddr = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDONLY, 0);
		if (mapAddr < 0)
		{
			DebugLogA("ERROR: Cannot open public shared file");
			return SCARD_F_INTERNAL_ERROR;
		}

		pageSize = SYS_GetPageSize();

		/*
		 * Allocate each reader structure 
		 */
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			/*
			 * Initially set the context, hcard structs to zero 
			 */
			psChannelMap[i].hCard = 0;
			psChannelMap[i].readerName = 0;
			psContextMap[i].hContext = 0;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;

			readerStates[i] = (PREADER_STATES)
				SYS_PublicMemoryMap(sizeof(READER_STATES),
				mapAddr, (i * pageSize));
			if (readerStates[i] == 0)
			{
				DebugLogA("ERROR: Cannot public memory map");
				close(mapAddr);	/* Close the memory map file */
				return SCARD_F_INTERNAL_ERROR;
			}
		}

		if (SHMClientSetupSession(parentPID) != 0)
		{
			close(mapAddr);
			return SCARD_E_NO_SERVICE;
		}

		atexit((void *) &SCardCleanupClient);

		isExecuted = 1;
	}

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{

		return SCARD_E_INVALID_VALUE;
	}

	scEstablishStruct.dwScope = dwScope;
	scEstablishStruct.phContext = 0;

	rv = WrapSHMWrite(SCARD_ESTABLISH_CONTEXT, parentPID,
		sizeof(scEstablishStruct), PCSCLITE_MCLIENT_ATTEMPTS,
		(void *) &scEstablishStruct);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
	{
		return SCARD_F_COMM_ERROR;
	}

	memcpy(&scEstablishStruct, &msgStruct.data, sizeof(scEstablishStruct));

	if (scEstablishStruct.rv != SCARD_S_SUCCESS)
	{
		return scEstablishStruct.rv;
	}

	*phContext = scEstablishStruct.phContext;

	/*
	 * Allocate the new hContext - if allocator full return an error 
	 */

	rv = SCardAddContext(*phContext);

	return rv;
}

/*
 * by najam 
 */
LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	long rv;

	SCardLockThread();
	rv = SCardReleaseContextTH(hContext);
	SCardUnlockThread();

	return rv;
}

/*
 * ------------by najam 
 */

static LONG SCardReleaseContextTH(SCARDCONTEXT hContext)
{

	LONG rv;
	release_struct scReleaseStruct;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	scReleaseStruct.hContext = hContext;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_RELEASE_CONTEXT, parentPID,
		sizeof(scReleaseStruct),
		PCSCLITE_MCLIENT_ATTEMPTS, (void *) &scReleaseStruct);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);
	memcpy(&scReleaseStruct, &msgStruct.data, sizeof(scReleaseStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	/*
	 * Remove the local context from the stack 
	 */
	SCardRemoveContext(hContext);

	return scReleaseStruct.rv;
}

LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout)
{

	/*
	 * Deprecated 
	 */

	return SCARD_S_SUCCESS;
}

LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	long rv;

	SCardLockThread();
	rv = SCardConnectTH(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;

}

static LONG SCardConnectTH(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{

	LONG rv;
	connect_struct scConnectStruct;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Check for NULL parameters 
	 */
	if (phCard == 0 || pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	} else
	{
		*phCard = 0;
	}

	if (szReader == 0)
	{
		return SCARD_E_UNKNOWN_READER;
	}

	/*
	 * Check for uninitialized strings 
	 */
	if (strlen(szReader) > MAX_READERNAME)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_INVALID_VALUE;
	}

	strncpy(scConnectStruct.szReader, szReader, MAX_READERNAME);

	scConnectStruct.hContext = hContext;
	scConnectStruct.dwShareMode = dwShareMode;
	scConnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scConnectStruct.phCard = *phCard;
	scConnectStruct.pdwActiveProtocol = *pdwActiveProtocol;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_CONNECT, parentPID,
		sizeof(scConnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scConnectStruct);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scConnectStruct, &msgStruct.data, sizeof(scConnectStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	*phCard = scConnectStruct.phCard;
	*pdwActiveProtocol = scConnectStruct.pdwActiveProtocol;

	if (scConnectStruct.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally 
		 */
		rv = SCardAddHandle(*phCard, (LPSTR) szReader);
		return rv;
	}

	return scConnectStruct.rv;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	long rv;

	SCardLockThread();
	rv = SCardReconnectTH(hCard, dwShareMode, dwPreferredProtocols,
		dwInitialization, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;

}

LONG SCardReconnectTH(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{

	LONG liIndex, rv;
	reconnect_struct scReconnectStruct;
	sharedSegmentMsg msgStruct;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	i = 0;
	rv = 0;

	if (dwInitialization != SCARD_LEAVE_CARD &&
		dwInitialization != SCARD_RESET_CARD &&
		dwInitialization != SCARD_UNPOWER_CARD &&
		dwInitialization != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	scReconnectStruct.hCard = hCard;
	scReconnectStruct.dwShareMode = dwShareMode;
	scReconnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scReconnectStruct.dwInitialization = dwInitialization;
	scReconnectStruct.pdwActiveProtocol = *pdwActiveProtocol;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_RECONNECT, parentPID,
		sizeof(scReconnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scReconnectStruct);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scReconnectStruct, &msgStruct.data, sizeof(scReconnectStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	*pdwActiveProtocol = scReconnectStruct.pdwActiveProtocol;

	return SCardCheckReaderAvailability(psChannelMap[liIndex].readerName,
		scReconnectStruct.rv);
}

/*
 * by najam 
 */
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{

	long rv;

	SCardLockThread();
	rv = SCardDisconnectTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;

}

/*
 * -----by najam 
 */

static LONG SCardDisconnectTH(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG liIndex, rv;
	disconnect_struct scDisconnectStruct;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	scDisconnectStruct.hCard = hCard;
	scDisconnectStruct.dwDisposition = dwDisposition;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_DISCONNECT, parentPID,
		sizeof(scDisconnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scDisconnectStruct);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scDisconnectStruct, &msgStruct.data,
		sizeof(scDisconnectStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	SCardRemoveHandle(hCard);

	return scDisconnectStruct.rv;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{

	LONG liIndex, rv;
	begin_struct scBeginStruct;
	int timeval, randnum, i, j;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	timeval = 0;
	randnum = 0;
	i = 0;
	rv = 0;

	/*
	 * change by najam 
	 */
	SCardLockThread();
	liIndex = SCardGetHandleIndice(hCard);
	/*
	 * change by najam 
	 */
	SCardUnlockThread();

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	scBeginStruct.hCard = hCard;

	/*
	 * Query the server every so often until the sharing violation ends
	 * and then hold the lock for yourself.  
	 */

	do
	{

		/*
		 * Look to see if it is locked before polling the server for
		 * admission to the readers resources 
		 */
		if ((readerStates[i])->lockState != 0)
		{
			for (j = 0; j < 100; j++)
			{
				/*
				 * This helps prevent starvation 
				 */
				randnum = SYS_Random(randnum, 1000.0, 10000.0);
				SYS_USleep(randnum);

				if ((readerStates[i])->lockState == 0)
				{
					break;
				}
			}
		}

		if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			return SCARD_E_NO_SERVICE;

		/*
		 * Begin lock 
		 */
		SCardLockThread();
		rv = WrapSHMWrite(SCARD_BEGIN_TRANSACTION, parentPID,
			sizeof(scBeginStruct),
			PCSCLITE_CLIENT_ATTEMPTS, (void *) &scBeginStruct);

		if (rv == -1)
		{
			/*
			 * End of lock 
			 */
			SCardUnlockThread();
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server 
		 */
		rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

		SCardUnlockThread();
		/*
		 * End of lock 
		 */

		memcpy(&scBeginStruct, &msgStruct.data, sizeof(scBeginStruct));

		if (rv == -1)
			return SCARD_F_COMM_ERROR;

	}
	while (scBeginStruct.rv == SCARD_E_SHARING_VIOLATION);

	return scBeginStruct.rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{

	long rv;

	SCardLockThread();
	rv = SCardEndTransactionTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;
}

LONG SCardEndTransactionTH(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG liIndex, rv;
	end_struct scEndStruct;
	sharedSegmentMsg msgStruct;
	int randnum, i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	randnum = 0;
	i = 0;
	rv = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	scEndStruct.hCard = hCard;
	scEndStruct.dwDisposition = dwDisposition;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_END_TRANSACTION, parentPID,
		sizeof(scEndStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scEndStruct);

	if (rv == -1)
	{

		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scEndStruct, &msgStruct.data, sizeof(scEndStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	/*
	 * This helps prevent starvation 
	 */
	randnum = SYS_Random(randnum, 1000.0, 10000.0);
	SYS_USleep(randnum);

	return scEndStruct.rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{

	long rv;

	SCardLockThread();
	rv = SCardCancelTransactionTH(hCard);
	SCardUnlockThread();
	return rv;
}

LONG SCardCancelTransactionTH(SCARDHANDLE hCard)
{

	LONG liIndex, rv;
	cancel_struct scCancelStruct;
	sharedSegmentMsg msgStruct;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	i = 0;
	rv = 0;

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
		return SCARD_E_READER_UNAVAILABLE;

	scCancelStruct.hCard = hCard;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_CANCEL_TRANSACTION, parentPID,
		sizeof(scCancelStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scCancelStruct);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scCancelStruct, &msgStruct.data, sizeof(scCancelStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	return scCancelStruct.rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	long rv;

	SCardLockThread();
	rv = SCardStatusTH(hCard, mszReaderNames, pcchReaderLen, pdwState,
		pdwProtocol, pbAtr, pcbAtrLen);
	SCardUnlockThread();
	return rv;
}

LONG SCardStatusTH(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{

	DWORD dwReaderLen;
	LONG liIndex, rv;
	int i;
	status_struct scStatusStruct;
	sharedSegmentMsg msgStruct;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	dwReaderLen = 0;
	rv = 0;
	i = 0;

	/*
	 * Check for NULL parameters 
	 */

	if (pcchReaderLen == 0 || pdwState == 0 ||
		pdwProtocol == 0 || pcbAtrLen == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * A small call must be made to pcscd to find out the event status of
	 * other applications such as reset/removed.  Only hCard is needed so
	 * I will not fill in the other information. 
	 */

	scStatusStruct.hCard = hCard;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_STATUS, parentPID,
		sizeof(scStatusStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scStatusStruct);

	if (rv == -1)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scStatusStruct, &msgStruct.data, sizeof(scStatusStruct));

	if (rv == -1)
	{
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_F_COMM_ERROR;
	}

	if (scStatusStruct.rv != SCARD_S_SUCCESS)
	{
		/*
		 * An event must have occurred 
		 */
		*pcchReaderLen = 0;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return scStatusStruct.rv;
	}

	/*
	 * Now continue with the client side SCardStatus 
	 */

	dwReaderLen = strlen(psChannelMap[liIndex].readerName) + 1;

	if (mszReaderNames == 0)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_S_SUCCESS;
	}

	if (*pcchReaderLen == 0)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_S_SUCCESS;
	}

	if (*pcchReaderLen < dwReaderLen)
	{
		*pcchReaderLen = dwReaderLen;
		*pcbAtrLen = 0;
		*pdwState = 0;
		*pdwProtocol = 0;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	*pcchReaderLen = dwReaderLen;
	*pdwState = (readerStates[i])->readerState;
	*pdwProtocol = (readerStates[i])->cardProtocol;
	*pcbAtrLen = (readerStates[i])->cardAtrLength;

	strcpy(mszReaderNames, psChannelMap[liIndex].readerName);
	memcpy(pbAtr, (readerStates[i])->cardAtr,
		(readerStates[i])->cardAtrLength);

	return SCARD_S_SUCCESS;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{

	LONG rv, contextIndice;
	PSCARD_READERSTATE_A currReader;
	PREADER_STATES rContext;
	LPSTR lpcReaderName;
	DWORD dwTime;
	DWORD dwState;
	DWORD dwBreakFlag;
	int i, j;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rContext = 0;
	lpcReaderName = 0;
	dwTime = 0;
	j = 0;
	dwState = 0;
	i = 0;
	currReader = 0;
	contextIndice = 0;
	dwBreakFlag = 0;

	if (rgReaderStates == 0 && cReaders > 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	if (cReaders < 0)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Make sure this context has been opened 
	 */

	/*
	 * change by najam 
	 */
	SCardLockThread();
	contextIndice = SCardGetContextIndice(hContext);
	/*
	 * change by najam 
	 */
	SCardUnlockThread();

	if (contextIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Application is waiting for a reader - return the first available
	 * reader 
	 */

	if (cReaders == 0)
	{
		while (1)
		{
			if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
				return SCARD_E_NO_SERVICE;

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if ((readerStates[i])->readerID != 0)
				{
					/*
					 * Reader was found 
					 */
					return SCARD_S_SUCCESS;
				}
			}

			if (dwTimeout == 0)
			{
				/*
				 * return immediately - no reader available 
				 */
				return SCARD_E_READER_UNAVAILABLE;
			}

			SYS_USleep(PCSCLITE_STATUS_WAIT);

			if (dwTimeout != INFINITE)
			{
				dwTime += PCSCLITE_STATUS_WAIT;

				if (dwTime >= (dwTimeout * 1000))
				{
					return SCARD_E_TIMEOUT;
				}
			}
		}
	} else if (cReaders > PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Check the integrity of the reader states structures 
	 */

	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		if (currReader->szReader == 0)
		{
			return SCARD_E_INVALID_VALUE;
		}
	}

	/*
	 * End of search for readers 
	 */

	/*
	 * Clear the event state for all readers 
	 */
	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		currReader->dwEventState = 0;
	}

	/*
	 * Now is where we start our event checking loop 
	 */

	/*
	 * DebugLogA("SCardGetStatusChange: Event Loop Start"_); 
	 */

	psContextMap[contextIndice].contextBlockStatus = BLOCK_STATUS_BLOCKING;

	j = 0;

	do
	{

		if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			return SCARD_E_NO_SERVICE;

		currReader = &rgReaderStates[j];

	/************ Look for IGNORED readers ****************************/

		if (currReader->dwCurrentState & SCARD_STATE_IGNORE)
		{
			currReader->dwEventState = SCARD_STATE_IGNORE;
		} else
		{

	  /************ Looks for correct readernames *********************/

			lpcReaderName = (char *) currReader->szReader;

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if (strcmp(lpcReaderName,
						(readerStates[i])->readerName) == 0)
				{
					break;
				}
			}

			/*
			 * The requested reader name is not recognized 
			 */
			if (i == PCSCLITE_MAX_CONTEXTS)
			{
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState = SCARD_STATE_UNKNOWN;
				} else
				{
					currReader->dwEventState =
						SCARD_STATE_UNKNOWN | SCARD_STATE_CHANGED;
					/*
					 * Spec says use SCARD_STATE_IGNORE but a removed USB
					 * reader with eventState fed into currentState will
					 * be ignored forever 
					 */
					dwBreakFlag = 1;
				}
			} else
			{

				/*
				 * The reader has come back after being away 
				 */
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					dwBreakFlag = 1;
				}

	/*****************************************************************/

				/*
				 * Set the reader status structure 
				 */
				rContext = readerStates[i];

				/*
				 * Now we check all the Reader States 
				 */
				dwState = rContext->readerState;

	/*********** Check if the reader is in the correct state ********/
				if (dwState & SCARD_UNKNOWN)
				{
					/*
					 * App thinks reader is in bad state and it is 
					 */
					if (currReader->
						dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState = SCARD_STATE_UNAVAILABLE;
					} else
					{
						/*
						 * App thinks reader is in good state and it is
						 * not 
						 */
						currReader->dwEventState = SCARD_STATE_CHANGED |
							SCARD_STATE_UNAVAILABLE;
						dwBreakFlag = 1;
					}
				} else
				{
					/*
					 * App thinks reader in bad state but it is not 
					 */
					if (currReader->
						dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState &=
							~SCARD_STATE_UNAVAILABLE;
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

	/********** Check for card presence in the reader **************/

				if (dwState & SCARD_PRESENT)
				{
					currReader->cbAtr = rContext->cardAtrLength;
					memcpy(currReader->rgbAtr, rContext->cardAtr,
						currReader->cbAtr);
				} else
				{
					currReader->cbAtr = 0;
				}

				/*
				 * Card is now absent 
				 */
				if (dwState & SCARD_ABSENT)
				{
					currReader->dwEventState |= SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_ATRMATCH;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;

					/*
					 * After present the rest are assumed 
					 */
					if (currReader->dwCurrentState & SCARD_STATE_PRESENT ||
						currReader->dwCurrentState & SCARD_STATE_ATRMATCH
						|| currReader->
						dwCurrentState & SCARD_STATE_EXCLUSIVE
						|| currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}

					/*
					 * Card is now present 
					 */
				} else if (dwState & SCARD_PRESENT)
				{
					currReader->dwEventState |= SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;

					if (currReader->dwCurrentState & SCARD_STATE_EMPTY)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}

					if (dwState & SCARD_SWALLOWED)
					{
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_MUTE;
						} else
						{
							currReader->dwEventState |= SCARD_STATE_MUTE;
							if (currReader->dwCurrentState !=
								SCARD_STATE_UNAWARE)
							{
								currReader->dwEventState |=
									SCARD_STATE_CHANGED;
							}
							dwBreakFlag = 1;
						}
					} else
					{
						/*
						 * App thinks card is mute but it is not 
						 */
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |=
								SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}

				/*
				 * Now figure out sharing modes 
				 */
				if (rContext->readerSharing == -1)
				{
					currReader->dwEventState |= SCARD_STATE_EXCLUSIVE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				} else if (rContext->readerSharing >= 1)
				{
					/*
					 * A card must be inserted for it to be INUSE 
					 */
					if (dwState & SCARD_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_INUSE;
						currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;
						if (currReader->
							dwCurrentState & SCARD_STATE_EXCLUSIVE)
						{
							currReader->dwEventState |=
								SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				} else if (rContext->readerSharing == 0)
				{
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;

					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					} else if (currReader->
						dwCurrentState & SCARD_STATE_EXCLUSIVE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

				if (currReader->dwCurrentState == SCARD_STATE_UNAWARE)
				{
					/*
					 * Break out of the while .. loop and return status
					 * once all the status's for all readers is met 
					 */
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					dwBreakFlag = 1;
				}

				SYS_USleep(PCSCLITE_STATUS_WAIT);

			}	/* End of SCARD_STATE_UNKNOWN */

		}	/* End of SCARD_STATE_IGNORE */

		/*
		 * Counter and resetter 
		 */
		j = j + 1;
		if (j == cReaders)
		{
			j = 0;
		}

		if (dwTimeout != INFINITE && dwTimeout != 0)
		{

			/*
			 * If time is greater than timeout and all readers have been
			 * checked 
			 */
			if ((dwTime >= (dwTimeout * 1000)) && (j == 0))
			{
				return SCARD_E_TIMEOUT;
			}

			dwTime += PCSCLITE_STATUS_WAIT;
		}

		/*
		 * Declare all the break conditions 
		 */

		if (psContextMap[contextIndice].contextBlockStatus ==
			BLOCK_STATUS_RESUME)
		{
			break;
		}

		/*
		 * Break if UNAWARE is set and all readers have been checked 
		 */
		if ((dwBreakFlag == 1) && (j == 0))
		{
			break;
		}

		/*
		 * Timeout has occurred and all readers checked 
		 */
		if ((dwTimeout == 0) && (j == 0))
		{
			break;
		}

	}
	while (1);

	/*
	 * DebugLogA("SCardGetStatusChange: Event Loop End"); 
	 */

	if (psContextMap[contextIndice].contextBlockStatus ==
		BLOCK_STATUS_RESUME)
	{
		return SCARD_E_CANCELLED;
	}

	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE hCard, LPCBYTE pbSendBuffer,
	DWORD cbSendLength, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{

	SCARD_IO_REQUEST pioSendPci, pioRecvPci;

	pioSendPci.dwProtocol = SCARD_PROTOCOL_RAW;
	pioRecvPci.dwProtocol = SCARD_PROTOCOL_RAW;

	return SCardTransmit(hCard, &pioSendPci, pbSendBuffer, cbSendLength,
		&pioRecvPci, pbRecvBuffer, pcbRecvLength);
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	long rv;

	SCardLockThread();
	rv = SCardTransmitTH(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	SCardUnlockThread();

	return rv;

}

/*
 * --------by najam 
 */

LONG SCardTransmitTH(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	LONG liIndex, rv;
	transmit_struct scTransmitStruct;
	sharedSegmentMsg msgStruct;
	int i;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	rv = 0;
	i = 0;

	if (pbSendBuffer == 0 || pbRecvBuffer == 0 ||
		pcbRecvLength == 0 || pioSendPci == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	liIndex = SCardGetHandleIndice(hCard);

	if (liIndex < 0)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (strcmp(psChannelMap[liIndex].readerName,
				(readerStates[i])->readerName) == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
		return SCARD_E_READER_UNAVAILABLE;

	if (cbSendLength > MAX_BUFFER_SIZE)
		return SCARD_E_INSUFFICIENT_BUFFER;

	scTransmitStruct.hCard = hCard;
	scTransmitStruct.cbSendLength = cbSendLength;
	scTransmitStruct.pcbRecvLength = *pcbRecvLength;
	memcpy(&scTransmitStruct.pioSendPci, pioSendPci,
		sizeof(SCARD_IO_REQUEST));
	memcpy(scTransmitStruct.pbSendBuffer, pbSendBuffer, cbSendLength);

	if (pioRecvPci)
	{
		memcpy(&scTransmitStruct.pioRecvPci, pioRecvPci,
			sizeof(SCARD_IO_REQUEST));
	} else
	{
		scTransmitStruct.pioRecvPci.dwProtocol = SCARD_PROTOCOL_ANY;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	rv = WrapSHMWrite(SCARD_TRANSMIT, parentPID,
		sizeof(scTransmitStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scTransmitStruct);

	if (rv == -1)
	{
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server 
	 */
	rv = SHMClientRead(&msgStruct, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scTransmitStruct, &msgStruct.data, sizeof(scTransmitStruct));

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	/*
	 * Zero it and free it so any secret information cannot be leaked 
	 */
	memset(scTransmitStruct.pbSendBuffer, 0x00, cbSendLength);

	if (scTransmitStruct.rv == SCARD_S_SUCCESS)
	{
		*pcbRecvLength = scTransmitStruct.pcbRecvLength;

		/*
		 * Copy and zero it so any secret information is not leaked 
		 */
		memcpy(pbRecvBuffer, scTransmitStruct.pbRecvBuffer,
			scTransmitStruct.pcbRecvLength);
		memset(scTransmitStruct.pbRecvBuffer, 0x00,
			scTransmitStruct.pcbRecvLength);

		if (pioRecvPci)
		{
			memcpy(pioRecvPci, &scTransmitStruct.pioRecvPci,
				sizeof(SCARD_IO_REQUEST));
		}

		return SCardCheckReaderAvailability(psChannelMap[liIndex].
			readerName, scTransmitStruct.rv);
	} else
	{
		*pcbRecvLength = scTransmitStruct.pcbRecvLength;
		return scTransmitStruct.rv;
	}
}

LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{

	long rv;

	SCardLockThread();
	rv = SCardListReadersTH(hContext, mszGroups, mszReaders, pcchReaders);
	SCardUnlockThread();

	return rv;
}

LONG SCardListReadersTH(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{

	LONG liIndex;
	DWORD dwGroupsLen, dwReadersLen;
	int i, lastChrPtr;

	/*
	 * Zero out everything 
	 */
	liIndex = 0;
	dwGroupsLen = 0;
	dwReadersLen = 0;
	i = 0;
	lastChrPtr = 0;

	/*
	 * Check for NULL parameters 
	 */
	if (pcchReaders == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((readerStates[i])->readerID != 0)
		{
			dwReadersLen += (strlen((readerStates[i])->readerName) + 1);
		}
	}

	dwReadersLen += 1;

	if (mszReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders < dwReadersLen)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_E_INSUFFICIENT_BUFFER;
	} else
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if ((readerStates[i])->readerID != 0)
			{
				/*
				 * Build the multi-string 
				 */
				strcpy(&mszReaders[lastChrPtr],
					(readerStates[i])->readerName);
				lastChrPtr += strlen((readerStates[i])->readerName);
				mszReaders[lastChrPtr] = 0;	/* Add the null */
				lastChrPtr += 1;
			}
		}
		mszReaders[lastChrPtr] = 0;	/* Add the last null */
	}

	*pcchReaders = dwReadersLen;
	return SCARD_S_SUCCESS;
}

LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{

	long rv;

	SCardLockThread();
	rv = SCardListReaderGroupsTH(hContext, mszGroups, pcchGroups);
	SCardUnlockThread();

	return rv;
}

/*
 * For compatibility purposes only 
 */
LONG SCardListReaderGroupsTH(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{

	LONG rv = SCARD_S_SUCCESS;

	const char ReaderGroup[] = "SCard$DefaultReaders";
	const int dwGroups = strlen(ReaderGroup) + 2;

	/*
	 * Make sure this context has been opened 
	 */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	if (mszGroups)
	{

		if (*pcchGroups < dwGroups)
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		else
		{
			memset(mszGroups, 0, dwGroups);
			memcpy(mszGroups, ReaderGroup, strlen(ReaderGroup));
		}
	}

	*pcchGroups = dwGroups;

	return rv;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{

	long rv;

	SCardLockThread();
	rv = SCardCancelTH(hContext);
	SCardUnlockThread();

	return rv;
}

LONG SCardCancelTH(SCARDCONTEXT hContext)
{

	LONG hContextIndice;

	hContextIndice = SCardGetContextIndice(hContext);

	if (hContextIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	/*
	 * Set the block status for this Context so blocking calls will
	 * complete 
	 */
	psContextMap[hContextIndice].contextBlockStatus = BLOCK_STATUS_RESUME;

	return SCARD_S_SUCCESS;
}

/*
 * Functions for managing instances of SCardEstablishContext These
 * functions keep track of Context handles and associate the blocking
 * variable contextBlockStatus to an hContext 
 */

static LONG SCardAddContext(SCARDCONTEXT hContext)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == hContext)
		{
			return SCARD_S_SUCCESS;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == 0)
		{
			psContextMap[i].hContext = hContext;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{

	int i;
	i = 0;

	/*
	 * Find this context and return it's spot in the array 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((hContext == psContextMap[i].hContext) && (hContext != 0))
		{
			return i;
		}
	}

	return -1;
}

static LONG SCardRemoveContext(SCARDCONTEXT hContext)
{

	LONG retIndice;
	retIndice = 0;

	retIndice = SCardGetContextIndice(hContext);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	} else
	{
		psContextMap[retIndice].hContext = 0;
		psContextMap[retIndice].contextBlockStatus = BLOCK_STATUS_RESUME;
		return SCARD_S_SUCCESS;
	}

}

/*
 * Functions for managing hCard values returned from SCardConnect. 
 */

LONG SCardGetHandleIndice(SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((hCard == psChannelMap[i].hCard) && (hCard != 0))
		{
			return i;
		}
	}

	return -1;
}

LONG SCardAddHandle(SCARDHANDLE hCard, LPSTR readerName)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psChannelMap[i].hCard == hCard)
		{
			return SCARD_S_SUCCESS;
		}
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (psChannelMap[i].hCard == 0)
		{
			psChannelMap[i].hCard = hCard;
			psChannelMap[i].readerName = strdup(readerName);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

LONG SCardRemoveHandle(SCARDHANDLE hCard)
{

	LONG retIndice;
	retIndice = 0;

	retIndice = SCardGetHandleIndice(hCard);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	} else
	{
		psChannelMap[retIndice].hCard = 0;
		free(psChannelMap[retIndice].readerName);
		return SCARD_S_SUCCESS;
	}
}

/*
 * This function sets up the mutex when the first call to EstablishContext 
 * is made by the first thread 
 */

/*
 * LONG SCardSetupThreadSafety() {
 * 
 * #ifdef USE_THREAD_SAFETY return 0; #else return 0; #endif }
 */

/*
 * This function locks a mutex so another thread must wait to use this
 * function 
 */

LONG SCardLockThread()
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexLock(&clientMutex);
#else
	return 0;
#endif

}

/*
 * This function unlocks a mutex so another thread may use the client
 * library 
 */

LONG SCardUnlockThread()
{

#ifdef USE_THREAD_SAFETY
	return SYS_MutexUnLock(&clientMutex);
#else
	return 0;
#endif

}

LONG SCardCheckDaemonAvailability()
{

	LONG rv;
	int fd;

	fd = open(PCSCLITE_SHM_FILE, O_RDWR);
	if (fd < 0)
	{
		DebugLogA("Error: Cannot open shared file " PCSCLITE_SHM_FILE);
	}

	/*
	 * Attempt a passive lock on the shared memory file 
	 */
	rv = SYS_LockFile(fd);

	close(fd);

	/*
	 * If the lock succeeds then the daemon is not there, otherwise if an 
	 * error is returned then it is 
	 */

	if (rv == 0)
	{
		return SCARD_E_NO_SERVICE;
	} else
	{
		return SCARD_S_SUCCESS;
	}
}

/*
 * This function takes an error response and checks to see if the reader
 * is still available if it is it returns the original errorCode, if not
 * it returns SCARD_E_READER_UNAVAILABLE 
 */

LONG SCardCheckReaderAvailability(LPSTR readerName, LONG errorCode)
{

	LONG retIndice;
	int i;

	retIndice = 0;
	i = 0;

	if (errorCode != SCARD_S_SUCCESS)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if (strcmp(psChannelMap[i].readerName, readerName) == 0)
			{
				return errorCode;
			}
		}

		return SCARD_E_READER_UNAVAILABLE;

	} else
	{
		return SCARD_S_SUCCESS;
	}

}

void SCardCleanupClient()
{

	/*
	 * Close down the client socket 
	 */
	SHMClientCloseSession();

}
