/*
 * This handles smartcard reader communications and
 * forwarding requests over message queues.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *
 * $Id$
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>

#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "debuglog.h"
#include "thread_generic.h"

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"

#include "winscard_msg.h"

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

/*
 * This is hack to allow a change of ATR at Reconnect without physically
 * changing the card. This should not happen in real life so should not be
 * problematic and so is active by default.
 */
#define ATR_CHANGE_AT_RECONNECT

struct _psChannelMap
{
	SCARDHANDLE hCard;
	LPSTR readerName;
};

typedef struct _psChannelMap CHANNEL_MAP, *PCHANNEL_MAP;

static struct _psContextMap
{
	DWORD dwClientID;
	SCARDCONTEXT hContext;
	DWORD contextBlockStatus;
	PCSCLITE_THREAD_T TID;            /* Thread owner of this context */
	PCSCLITE_MUTEX_T mMutex;          /* Mutex for this context */
	CHANNEL_MAP psChannelMap[PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS];
} psContextMap[PCSCLITE_MAX_APPLICATION_CONTEXTS];

static short isExecuted = 0;
static int mapAddr = 0;

static PCSCLITE_MUTEX clientMutex = PTHREAD_MUTEX_INITIALIZER;

static PREADER_STATES readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };


static LONG SCardAddContext(SCARDCONTEXT, DWORD);
static LONG SCardGetContextIndice(SCARDCONTEXT);
static LONG SCardGetContextIndiceTH(SCARDCONTEXT);
static LONG SCardRemoveContext(SCARDCONTEXT);

static LONG SCardAddHandle(SCARDHANDLE, DWORD, LPSTR);
static LONG SCardGetIndicesFromHandle(SCARDHANDLE, PDWORD, PDWORD);
static LONG SCardGetIndicesFromHandleTH(SCARDHANDLE, PDWORD, PDWORD);
static LONG SCardRemoveHandle(SCARDHANDLE);

static LONG SCardGetSetAttrib(SCARDHANDLE hCard, int command, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen);

static LONG SCardCheckDaemonAvailability();

/*
 * Thread safety functions
 */
static LONG SCardLockThread();
static LONG SCardUnlockThread();

static LONG SCardEstablishContextTH(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT);

LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

	return rv;
}

static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;
	int i, j, pageSize;
	establish_struct scEstablishStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwClientID = 0;

	/*
	 * Zero out everything
	 */
	rv = 0;
	i = 0;
	j = 0;
	pageSize = 0;

	if (phContext == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phContext = 0;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Do this only once
	 */
	if (isExecuted == 0)
	{
		/*
		 * Initialize debug
		 */
		if (getenv("MUSCLECARD_DEBUG"))
			DebugLogSetLogType(DEBUGLOG_STDERR_DEBUG);

		/*
		 * Do any system initilization here
		 */
		SYS_Initialize();

		/*
		 * Set up the memory mapped reader stats structures
		 */
		mapAddr = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDONLY, 0);
		if (mapAddr < 0)
		{
			DebugLogB("ERROR: Cannot open public shared file: %s",
				PCSCLITE_PUBSHM_FILE);
			return SCARD_E_NO_SERVICE;
		}

		pageSize = SYS_GetPageSize();

		/*
		 * Allocate each reader structure
		 */
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			readerStates[i] = (PREADER_STATES)
				SYS_PublicMemoryMap(sizeof(READER_STATES),
				mapAddr, (i * pageSize));
			if (readerStates[i] == NULL)
			{
				DebugLogA("ERROR: Cannot public memory map");
				SYS_CloseFile(mapAddr);	/* Close the memory map file */
				return SCARD_F_INTERNAL_ERROR;
			}
		}

		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			/*
			 * Initially set the context struct to zero
			 */
			psContextMap[i].dwClientID = 0;
			psContextMap[i].hContext = 0;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			psContextMap[i].mMutex = 0;

			for (j = 0; j < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; j++)
			{
				/*
				 * Initially set the hcard structs to zero
				 */
				psContextMap[i].psChannelMap[j].hCard = 0;
				psContextMap[i].psChannelMap[j].readerName = NULL;
			}
		}

	}

	/*
	 * Is there a free slot for this connection ?
	 */

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].dwClientID == 0)
			break;
	}

	if (i == PCSCLITE_MAX_APPLICATION_CONTEXTS)
	{
		return SCARD_E_NO_MEMORY;
	}

	if (SHMClientSetupSession(&dwClientID) != 0)
	{
		SYS_CloseFile(mapAddr);
		return SCARD_E_NO_SERVICE;
	}

	{	/* exchange client/server protocol versions */
		sharedSegmentMsg msgStruct;
		version_struct *veStr;

		msgStruct.mtype = CMD_VERSION;
		msgStruct.user_id = SYS_GetUID();
		msgStruct.group_id = SYS_GetGID();
		msgStruct.command = 0;
		msgStruct.date = time(NULL);

		veStr = (version_struct *) msgStruct.data;
		veStr->major = PROTOCOL_VERSION_MAJOR;
		veStr->minor = PROTOCOL_VERSION_MINOR;

		if (-1 == SHMMessageSend(&msgStruct, dwClientID,
			PCSCLITE_MCLIENT_ATTEMPTS))
			return SCARD_E_NO_SERVICE;

		/*
		 * Read a message from the server
		 */
		if (-1 == SHMMessageReceive(&msgStruct, dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS))
		{
			DebugLogA("Your pcscd is too old and does not support CMD_VERSION");
			return SCARD_F_COMM_ERROR;
		}

		DebugLogC("Server is protocol version %d:%d",
			veStr->major, veStr->minor);

		isExecuted = 1;
	}


	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{
		return SCARD_E_INVALID_VALUE;
	}

	scEstablishStruct.dwScope = dwScope;
	scEstablishStruct.phContext = 0;

	rv = WrapSHMWrite(SCARD_ESTABLISH_CONTEXT, dwClientID,
		sizeof(scEstablishStruct), PCSCLITE_MCLIENT_ATTEMPTS,
		(void *) &scEstablishStruct);

	if (rv == -1)
		return SCARD_E_NO_SERVICE;

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	memcpy(&scEstablishStruct, &msgStruct.data, sizeof(scEstablishStruct));

	if (scEstablishStruct.rv != SCARD_S_SUCCESS)
		return scEstablishStruct.rv;

	*phContext = scEstablishStruct.phContext;

	/*
	 * Allocate the new hContext - if allocator full return an error
	 */

	rv = SCardAddContext(*phContext, dwClientID);

	return rv;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG rv;
	release_struct scReleaseStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex;
	PCSCLITE_THREAD_T currentTID;

	/*
	 * Zero out everything
	 */
	rv = 0;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	/*
	 * Test if the thread that would release the context is the thread owning this context
	 */
	currentTID = SYS_ThreadSelf();
	rv = SYS_ThreadEqual(&psContextMap[dwContextIndex].TID, &currentTID);
	
	if (rv == 0)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		/* Perhaps there is a better error code */
		return SCARD_F_INTERNAL_ERROR;
	}

	scReleaseStruct.hContext = hContext;

	rv = WrapSHMWrite(SCARD_RELEASE_CONTEXT, psContextMap[dwContextIndex].dwClientID,
			  sizeof(scReleaseStruct),
			  PCSCLITE_MCLIENT_ATTEMPTS, (void *) &scReleaseStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);
	memcpy(&scReleaseStruct, &msgStruct.data, sizeof(scReleaseStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}
	
	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	/*
	 * Remove the local context from the stack
	 */
	SCardLockThread();
	SCardRemoveContext(hContext);
	SCardUnlockThread();

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
	LONG rv;
	connect_struct scConnectStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;

	/*
	 * Check for NULL parameters
	 */
	if (phCard == 0 || pdwActiveProtocol == 0)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phCard = 0;

	if (szReader == 0)
		return SCARD_E_UNKNOWN_READER;

	/*
	 * Check for uninitialized strings
	 */
	if (strlen(szReader) > MAX_READERNAME)
		return SCARD_E_INVALID_VALUE;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{
		return SCARD_E_INVALID_VALUE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	strncpy(scConnectStruct.szReader, szReader, MAX_READERNAME);

	scConnectStruct.hContext = hContext;
	scConnectStruct.dwShareMode = dwShareMode;
	scConnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scConnectStruct.phCard = *phCard;
	scConnectStruct.pdwActiveProtocol = *pdwActiveProtocol;

	rv = WrapSHMWrite(SCARD_CONNECT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scConnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scConnectStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scConnectStruct, &msgStruct.data, sizeof(scConnectStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	*phCard = scConnectStruct.phCard;
	*pdwActiveProtocol = scConnectStruct.pdwActiveProtocol;

	if (scConnectStruct.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally
		 */
		rv = SCardAddHandle(*phCard, dwContextIndex, (LPSTR) szReader);
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return rv;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
	return scConnectStruct.rv;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	reconnect_struct scReconnectStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
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
		return SCARD_E_INVALID_PARAMETER;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	


	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	scReconnectStruct.hCard = hCard;
	scReconnectStruct.dwShareMode = dwShareMode;
	scReconnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scReconnectStruct.dwInitialization = dwInitialization;
	scReconnectStruct.pdwActiveProtocol = *pdwActiveProtocol;

	rv = WrapSHMWrite(SCARD_RECONNECT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scReconnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scReconnectStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scReconnectStruct, &msgStruct.data, sizeof(scReconnectStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	*pdwActiveProtocol = scReconnectStruct.pdwActiveProtocol;

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
	
	return scReconnectStruct.rv;
}

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	disconnect_struct scDisconnectStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{
		return SCARD_E_INVALID_VALUE;
	}

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	scDisconnectStruct.hCard = hCard;
	scDisconnectStruct.dwDisposition = dwDisposition;

	rv = WrapSHMWrite(SCARD_DISCONNECT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scDisconnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scDisconnectStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scDisconnectStruct, &msgStruct.data,
		sizeof(scDisconnectStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	SCardRemoveHandle(hCard);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return scDisconnectStruct.rv;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{

	LONG rv;
	begin_struct scBeginStruct;
	int timeval, randnum, i, j;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	timeval = 0;
	randnum = 0;
	i = 0;
	rv = 0;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
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

		rv = WrapSHMWrite(SCARD_BEGIN_TRANSACTION, psContextMap[dwContextIndex].dwClientID,
			sizeof(scBeginStruct),
			PCSCLITE_CLIENT_ATTEMPTS, (void *) &scBeginStruct);

		if (rv == -1)
		{
			
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);


		memcpy(&scBeginStruct, &msgStruct.data, sizeof(scBeginStruct));

		if (rv == -1)
		{
			
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
			return SCARD_F_COMM_ERROR;
		}

	}
	while (scBeginStruct.rv == SCARD_E_SHARING_VIOLATION);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
	return scBeginStruct.rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	end_struct scEndStruct;
	sharedSegmentMsg msgStruct;
	int randnum, i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
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

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	scEndStruct.hCard = hCard;
	scEndStruct.dwDisposition = dwDisposition;

	rv = WrapSHMWrite(SCARD_END_TRANSACTION, psContextMap[dwContextIndex].dwClientID,
		sizeof(scEndStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scEndStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scEndStruct, &msgStruct.data, sizeof(scEndStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	/*
	 * This helps prevent starvation
	 */
	randnum = SYS_Random(randnum, 1000.0, 10000.0);
	SYS_USleep(randnum);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return scEndStruct.rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	cancel_struct scCancelStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	i = 0;
	rv = 0;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	scCancelStruct.hCard = hCard;

	rv = WrapSHMWrite(SCARD_CANCEL_TRANSACTION, psContextMap[dwContextIndex].dwClientID,
		sizeof(scCancelStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scCancelStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scCancelStruct, &msgStruct.data, sizeof(scCancelStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return scCancelStruct.rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	DWORD dwReaderLen, dwAtrLen;
	LONG rv;
	int i;
	status_struct scStatusStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;
	i = 0;

	/*
	 * Check for NULL parameters
	 */

	if (pcchReaderLen == NULL || pdwState == NULL || pdwProtocol == NULL
		|| pcbAtrLen == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/* length passed from caller */
	dwReaderLen = *pcchReaderLen;
	dwAtrLen = *pcbAtrLen;

	/* default output values */
	*pdwState = 0;
	*pdwProtocol = 0;
	*pcchReaderLen = 0;
	*pcbAtrLen = 0;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	/* initialise the structure */
	memset(&scStatusStruct, 0, sizeof(scStatusStruct));
	scStatusStruct.hCard = hCard;

	/* those sizes need to be initialised */
	scStatusStruct.pcchReaderLen = sizeof(scStatusStruct.mszReaderNames);
	scStatusStruct.pcbAtrLen = sizeof(scStatusStruct.pbAtr);

	rv = WrapSHMWrite(SCARD_STATUS, psContextMap[dwContextIndex].dwClientID,
		sizeof(scStatusStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scStatusStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scStatusStruct, &msgStruct.data, sizeof(scStatusStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	rv = scStatusStruct.rv;
	if (rv != SCARD_S_SUCCESS && rv != SCARD_E_INSUFFICIENT_BUFFER)
	{
		/*
		 * An event must have occurred
		 */
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return rv;
	}

	/*
	 * Now continue with the client side SCardStatus
	 */

	*pcchReaderLen = strlen(psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName) + 1;
#ifdef ATR_CHANGE_AT_RECONNECT
	*pcbAtrLen = scStatusStruct.pcbAtrLen;
#else
	*pcbAtrLen = (readerStates[i])->cardAtrLength;
#endif

	*pdwState = (readerStates[i])->readerState;
	*pdwProtocol = (readerStates[i])->cardProtocol;

	/* return SCARD_E_INSUFFICIENT_BUFFER only if buffer pointer is non NULL */
	if (mszReaderNames)
	{
		if (*pcchReaderLen > dwReaderLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;

		strncpy(mszReaderNames, 
			psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName, 
			dwReaderLen);
	}

	if (pbAtr)
	{
		if (*pcbAtrLen > dwAtrLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;

#ifdef ATR_CHANGE_AT_RECONNECT
		memcpy(pbAtr, scStatusStruct.pbAtr,
			min(*pcbAtrLen, dwAtrLen));
#else
		memcpy(pbAtr, (readerStates[i])->cardAtr,
			min(*pcbAtrLen, dwAtrLen));
#endif
	}
	
	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return rv;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{
	LONG rv;
	PSCARD_READERSTATE_A currReader;
	PREADER_STATES rContext;
	LPSTR lpcReaderName;
	DWORD dwTime;
	DWORD dwState;
	DWORD dwBreakFlag;
	int i, j;
	DWORD dwContextIndex;

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
	dwContextIndex = 0;
	dwBreakFlag = 0;

	if (rgReaderStates == 0 && cReaders > 0)
		return SCARD_E_INVALID_PARAMETER;

	if (cReaders < 0)
		return SCARD_E_INVALID_VALUE;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this context has been opened
	 */

	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	/*
	 * Application is waiting for a reader - return the first available
	 * reader
	 */

	if (cReaders == 0)
	{
		while (1)
		{
			if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
			{
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
				return SCARD_E_NO_SERVICE;
			}

			for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
			{
				if ((readerStates[i])->readerID != 0)
				{
					/*
					 * Reader was found
					 */
					SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
					return SCARD_S_SUCCESS;
				}
			}

			if (dwTimeout == 0)
			{
				/*
				 * return immediately - no reader available
				 */
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
				return SCARD_E_READER_UNAVAILABLE;
			}

			SYS_USleep(PCSCLITE_STATUS_WAIT);

			if (dwTimeout != INFINITE)
			{
				dwTime += PCSCLITE_STATUS_WAIT;

				if (dwTime >= (dwTimeout * 1000))
				{
					SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
					return SCARD_E_TIMEOUT;
				}
			}
		}
	}
	else
		if (cReaders >= PCSCLITE_MAX_READERS_CONTEXTS)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
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
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
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

	psContextMap[dwContextIndex].contextBlockStatus = BLOCK_STATUS_BLOCKING;

	j = 0;

	do
	{
		if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
			return SCARD_E_NO_SERVICE;
		}

		currReader = &rgReaderStates[j];

	/************ Look for IGNORED readers ****************************/

		if (currReader->dwCurrentState & SCARD_STATE_IGNORE)
		{
			currReader->dwEventState = SCARD_STATE_IGNORE;
		} else
		{

	  /************ Looks for correct readernames *********************/

			lpcReaderName = (char *) currReader->szReader;

			for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
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
			if (i == PCSCLITE_MAX_READERS_CONTEXTS)
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
			j = 0;

		if (dwTimeout != INFINITE && dwTimeout != 0)
		{
			/*
			 * If time is greater than timeout and all readers have been
			 * checked
			 */
			if ((dwTime >= (dwTimeout * 1000)) && (j == 0))
			{
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
				return SCARD_E_TIMEOUT;
			}

			dwTime += PCSCLITE_STATUS_WAIT;
		}

		/*
		 * Declare all the break conditions
		 */

		if (psContextMap[dwContextIndex].contextBlockStatus ==
			BLOCK_STATUS_RESUME)
			break;

		/*
		 * Break if UNAWARE is set and all readers have been checked
		 */
		if ((dwBreakFlag == 1) && (j == 0))
			break;

		/*
		 * Timeout has occurred and all readers checked
		 */
		if ((dwTimeout == 0) && (j == 0))
			break;

	}
	while (1);

	/*
	 * DebugLogA("SCardGetStatusChange: Event Loop End");
	 */

	if (psContextMap[dwContextIndex].contextBlockStatus ==
			BLOCK_STATUS_RESUME)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_CANCELLED;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer,
	DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	LONG rv;
	control_struct scControlStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;
	i = 0;

	/* 0 bytes received by default */
	if (NULL != lpBytesReturned)
		*lpBytesReturned = 0;

	if (pbSendBuffer == 0)
		return SCARD_E_INVALID_PARAMETER;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	if (cbSendLength > MAX_BUFFER_SIZE)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	scControlStruct.hCard = hCard;
	scControlStruct.dwControlCode = dwControlCode;
	scControlStruct.cbSendLength = cbSendLength;
	scControlStruct.cbRecvLength = cbRecvLength;
	memcpy(scControlStruct.pbSendBuffer, pbSendBuffer, cbSendLength);

	rv = WrapSHMWrite(SCARD_CONTROL, psContextMap[dwContextIndex].dwClientID,
		sizeof(scControlStruct), PCSCLITE_CLIENT_ATTEMPTS, &scControlStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	memcpy(&scControlStruct, &msgStruct.data, sizeof(scControlStruct));

	if (NULL != lpBytesReturned)
		*lpBytesReturned = scControlStruct.dwBytesReturned;

	if (scControlStruct.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Copy and zero it so any secret information is not leaked
		 */
		memcpy(pbRecvBuffer, scControlStruct.pbRecvBuffer,
			scControlStruct.cbRecvLength);
		memset(scControlStruct.pbRecvBuffer, 0x00,
			sizeof(scControlStruct.pbRecvBuffer));
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		
	return scControlStruct.rv;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
	LPDWORD pcbAttrLen)
{
	return SCardGetSetAttrib(hCard, SCARD_GET_ATTRIB, dwAttrId, pbAttr,
		pcbAttrLen);
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
	DWORD cbAttrLen)
{
	return SCardGetSetAttrib(hCard, SCARD_SET_ATTRIB, dwAttrId, (LPBYTE)pbAttr,
		&cbAttrLen);
}

LONG SCardGetSetAttrib(SCARDHANDLE hCard, int command, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG rv;
	getset_struct scGetSetStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;
	i = 0;

	if (NULL == pbAttr || 0 == *pcbAttrLen)
		return SCARD_E_INVALID_PARAMETER;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	if (*pcbAttrLen > MAX_BUFFER_SIZE)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	scGetSetStruct.hCard = hCard;
	scGetSetStruct.dwAttrId = dwAttrId;
	scGetSetStruct.cbAttrLen = *pcbAttrLen;
	scGetSetStruct.rv = SCARD_E_NO_SERVICE;
	memcpy(scGetSetStruct.pbAttr, pbAttr, *pcbAttrLen);

	rv = WrapSHMWrite(command,
		psContextMap[dwContextIndex].dwClientID, sizeof(scGetSetStruct),
		PCSCLITE_CLIENT_ATTEMPTS, &scGetSetStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

	memcpy(&scGetSetStruct, &msgStruct.data, sizeof(scGetSetStruct));

	if ((SCARD_S_SUCCESS == scGetSetStruct.rv) && (SCARD_GET_ATTRIB == command))
	{
		/*
		 * Copy and zero it so any secret information is not leaked
		 */
		if (*pcbAttrLen < scGetSetStruct.cbAttrLen)
		{
			scGetSetStruct.cbAttrLen = *pcbAttrLen;
			scGetSetStruct.rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
		else
			*pcbAttrLen = scGetSetStruct.cbAttrLen;

		memcpy(pbAttr, scGetSetStruct.pbAttr, scGetSetStruct.cbAttrLen);
		memset(scGetSetStruct.pbAttr, 0x00, sizeof(scGetSetStruct.pbAttr));
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	

	return scGetSetStruct.rv;
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	transmit_struct scTransmitStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	/*
	 * Zero out everything
	 */
	rv = 0;
	i = 0;

	if (pbSendBuffer == 0 || pbRecvBuffer == 0 ||
			pcbRecvLength == 0 || pioSendPci == 0)
		return SCARD_E_INVALID_PARAMETER;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);

	if (rv == -1)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_READER_UNAVAILABLE;
	}

	if (cbSendLength > MAX_BUFFER_SIZE)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

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
	}
	else
		scTransmitStruct.pioRecvPci.dwProtocol = SCARD_PROTOCOL_ANY;

	rv = WrapSHMWrite(SCARD_TRANSMIT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scTransmitStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scTransmitStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scTransmitStruct, &msgStruct.data, sizeof(scTransmitStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_F_COMM_ERROR;
	}

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

		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		
		return scTransmitStruct.rv;
	} else
	{
		*pcbRecvLength = scTransmitStruct.pcbRecvLength;
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return scTransmitStruct.rv;
	}
}

LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	DWORD dwGroupsLen, dwReadersLen;
	int i, lastChrPtr;
	DWORD dwContextIndex;

	/*
	 * Zero out everything
	 */
	dwGroupsLen = 0;
	dwReadersLen = 0;
	i = 0;
	lastChrPtr = 0;

	/*
	 * Check for NULL parameters
	 */
	if (pcchReaders == 0)
		return SCARD_E_INVALID_PARAMETER;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
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
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_S_SUCCESS;
	} else if (*pcchReaders < dwReadersLen)
	{
		*pcchReaders = dwReadersLen;
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
		return SCARD_E_INSUFFICIENT_BUFFER;
	} else
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
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

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
	return SCARD_S_SUCCESS;
}

LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{
	LONG rv = SCARD_S_SUCCESS;
	DWORD dwContextIndex;

	const char ReaderGroup[] = "SCard$DefaultReaders";
	const int dwGroups = strlen(ReaderGroup) + 2;

	if (SCardCheckDaemonAvailability() != SCARD_S_SUCCESS)
		return SCARD_E_NO_SERVICE;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

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

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);	
	return rv;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{
	DWORD dwContextIndex;

	dwContextIndex = SCardGetContextIndice(hContext);

	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);	

	/*
	 * Set the block status for this Context so blocking calls will
	 * complete
	 */
	psContextMap[dwContextIndex].contextBlockStatus = BLOCK_STATUS_RESUME;

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	return SCARD_S_SUCCESS;
}

/*
 * Functions for managing instances of SCardEstablishContext These
 * functions keep track of Context handles and associate the blocking
 * variable contextBlockStatus to an hContext
 */

static LONG SCardAddContext(SCARDCONTEXT hContext, DWORD dwClientID)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == 0)
		{
			psContextMap[i].hContext = hContext;
			psContextMap[i].TID = SYS_ThreadSelf();
			psContextMap[i].dwClientID = dwClientID;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			psContextMap[i].mMutex = (PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
			SYS_MutexInit(psContextMap[i].mMutex);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{
	LONG rv;

	SCardLockThread();
	rv = SCardGetContextIndiceTH(hContext);
	SCardUnlockThread();

	return rv;
}


static LONG SCardGetContextIndiceTH(SCARDCONTEXT hContext)
{
	int i;
	i = 0;

	/*
	 * Find this context and return it's spot in the array
	 */
	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if ((hContext == psContextMap[i].hContext) && (hContext != 0))
			return i;
	}

	return -1;
}

static LONG SCardRemoveContext(SCARDCONTEXT hContext)
{
	LONG  retIndice;
	int i;

	retIndice = SCardGetContextIndiceTH(hContext);

	if (retIndice == -1)
		return SCARD_E_INVALID_HANDLE;
	else
	{
		psContextMap[retIndice].hContext = 0;
		SHMClientCloseSession(psContextMap[retIndice].dwClientID);
		psContextMap[retIndice].dwClientID = 0;
		free(psContextMap[retIndice].mMutex);
		psContextMap[retIndice].mMutex = 0;
		psContextMap[retIndice].contextBlockStatus = BLOCK_STATUS_RESUME;

		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
		{
			/*
			 * Initially set the hcard structs to zero
			 */
			psContextMap[retIndice].psChannelMap[i].hCard = 0;
			free(psContextMap[retIndice].psChannelMap[i].readerName);
			psContextMap[retIndice].psChannelMap[i].readerName = NULL;
		}

		return SCARD_S_SUCCESS;
	}
}

/*
 * Functions for managing hCard values returned from SCardConnect.
 */

LONG SCardAddHandle(SCARDHANDLE hCard, DWORD dwContextIndex, LPSTR readerName)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContextMap[dwContextIndex].psChannelMap[i].hCard == 0)
		{
			psContextMap[dwContextIndex].psChannelMap[i].hCard = hCard;
			psContextMap[dwContextIndex].psChannelMap[i].readerName = strdup(readerName);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

LONG SCardRemoveHandle(SCARDHANDLE hCard)
{
	DWORD dwContextIndice, dwChannelIndice;
	LONG rv;

	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndice, &dwChannelIndice);

	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;
	else
	{
		psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].hCard = 0;
		free(psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].readerName);
		psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].readerName = NULL;
		return SCARD_S_SUCCESS;
	}
}

static LONG SCardGetIndicesFromHandle(SCARDHANDLE hCard, PDWORD pdwContextIndice, PDWORD pdwChannelIndice)
{
	LONG rv;

	SCardLockThread();
	rv = SCardGetIndicesFromHandleTH(hCard, pdwContextIndice, pdwChannelIndice);
	SCardUnlockThread();

	return rv;
}

static LONG SCardGetIndicesFromHandleTH(SCARDHANDLE hCard, PDWORD pdwContextIndice, PDWORD pdwChannelIndice)
{
	int i, j;

	i = 0;
	j = 0;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext != 0)
		{
			for (j = 0; j < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; j++)
			{
				if (psContextMap[i].psChannelMap[j].hCard == hCard)
				{
					*pdwContextIndice = i;
					*pdwChannelIndice = j;
					return SCARD_S_SUCCESS;
				}
			}
			
		}
	}

	return -1;
}

/*
 * This function locks a mutex so another thread must wait to use this
 * function
 */

LONG SCardLockThread()
{
	return SYS_MutexLock(&clientMutex);
}

/*
 * This function unlocks a mutex so another thread may use the client
 * library
 */

LONG SCardUnlockThread()
{
	return SYS_MutexUnLock(&clientMutex);
}

LONG SCardCheckDaemonAvailability()
{
	LONG rv;
	struct stat statBuffer;

	rv = SYS_Stat(PCSCLITE_IPC_DIR, &statBuffer);

	if (rv != 0)
	{
		DebugLogA("SCardCheckDaemonAvailability: PCSC Not Running");
		return SCARD_E_NO_SERVICE;
	}

	return SCARD_S_SUCCESS;
}

/*
 * free resources allocated by the library
 * You _shall_ call this function if you use dlopen/dlclose to load/unload the
 * library. Otherwise you will exhaust the ressources available.
 */
void SCardUnload(void)
{
	if (!isExecuted)
		return;

	SYS_CloseFile(mapAddr);
	isExecuted = 0;
}

