/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2002
 *  David Corcoran <corcoran@linuxnet.com>
 *  Najam Siddiqui
 * Copyright (C) 2005
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides PC/SC to SCF shimming.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <smartcard/scf.h>
#include <time.h>

#include "pcsclite.h"
#include "winscard.h"
#include "debug.h"

#include "thread_generic.h"

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"

#define TRUE	1
#define FALSE	0

#undef PCSCLITE_MAX_READERS_CONTEXTS
#define PCSCLITE_MAX_READERS_CONTEXTS	2

/* Global session to manage Readers, Card events. */
static SCF_Session_t g_hSession = NULL;

/* Have to define this because they are defined in pcsclite.h as externs */
SCARD_IO_REQUEST g_rgSCardT0Pci, g_rgSCardT1Pci, g_rgSCardRawPci;

static struct _psTransmitMap
{
	BYTE Buffer[266];
	int isResponseCached;
	LONG bufferLength;
} psTransmitMap[PCSCLITE_MAX_APPLICATION_CONTEXTS];

/* Channel Map to manage Card Connections. */
static struct _psChannelMap
{
	SCARDHANDLE PCSC_hCard;
	SCARDCONTEXT hContext;
	SCF_Session_t hSession;
	SCF_Terminal_t hTerminal;
	SCF_Card_t SCF_hCard;
	short haveLock;
	short isReset;
	int ReaderIndice;
} psChannelMap[PCSCLITE_MAX_APPLICATION_CONTEXTS];

/* Context Map to manage contexts and sessions. */
static struct _psContextMap
{
	SCARDCONTEXT hContext;
	SCF_Session_t hSession;
	DWORD contextBlockStatus;
} psContextMap[PCSCLITE_MAX_APPLICATION_CONTEXTS];

/* Reader Map to handle Status and GetStatusChange. */
static struct _psReaderMap
{
	SCF_Terminal_t hTerminal;
	LPSTR ReaderName;
	short SharedRefCount;
	DWORD dwCurrentState;
	BYTE bAtr[MAX_ATR_SIZE];
	DWORD dwAtrLength;
	SCF_ListenerHandle_t lHandle;
} psReaderMap[PCSCLITE_MAX_READERS_CONTEXTS];

static PCSCLITE_MUTEX clientMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Mutex for the Smardcard Event Handling, different from client Mutex
 * because event reporting from the ocfserver is done using a single thread,
 * so to get lock on the clientMutex may affect the performance of the ocf server.
 */
static PCSCLITE_MUTEX EventMutex = PTHREAD_MUTEX_INITIALIZER;
static PCSCLITE_MUTEX SCFInitMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t EventCondition = PTHREAD_COND_INITIALIZER;
static char PCSC_Initialized = 0;

static LONG isOCFServerRunning(void);
LONG SCardLockThread(void);
LONG SCardUnlockThread(void);
LONG SCardEventLock(void);
LONG SCardEventUnlock(void);
static LONG PCSC_SCF_Initialize(void);
static void EventCallback(SCF_Event_t eventType, SCF_Terminal_t hTerm,
	void *cbdata);
static LONG PCSC_SCF_getATR(SCF_Card_t hCard, LPBYTE pcbAtr,
	LPDWORD pcbAtrLen);

static LONG ConvertStatus(SCF_Status_t status);
static LONG SCardGetReaderIndice(LPCSTR ReaderName);
static LONG getNewContext(SCARDCONTEXT * phContext);
static LONG SCardAddContext(SCARDCONTEXT hContext, SCF_Session_t hSession);
static SCF_Session_t getSessionForContext(SCARDCONTEXT hContext);
static LONG SCardRemoveContext(SCARDCONTEXT hContext);
static LONG SCardGetContextIndice(SCARDCONTEXT hContext);

static LONG getNewHandle(SCARDCONTEXT hContext, LPCSTR szReader,
	SCARDHANDLE * phCard, DWORD);
static LONG getCardForHandle(SCARDHANDLE PSCS_hCard, SCF_Card_t * SCF_hCard);
static LONG SCardRemoveHandle(SCARDHANDLE hCard);
static LONG SCardAddHandle(SCARDHANDLE PCSC_hCard, SCARDCONTEXT hContext,
	SCF_Session_t hSession, SCF_Terminal_t hTerminal,
	SCF_Card_t SCF_hCard, int, DWORD);
static LONG SCardGetHandleIndice(SCARDHANDLE hCard);
static LONG isActiveContextPresent(void);


static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv = 0;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	rv = PCSC_SCF_Initialize();

	if (SCARD_S_SUCCESS != rv)
		return rv;

	if (NULL == phContext)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phContext = 0;

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{
		*phContext = 0;
		return SCARD_E_INVALID_VALUE;
	}
	rv = getNewContext(phContext);
	return rv;
}

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

static LONG SCardReleaseContextTH(SCARDCONTEXT hContext)
{
	LONG rv;
	/* Zero out everything */

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	rv = 0;

	/* Remove the local context from the stack */
	rv = SCardRemoveContext(hContext);

	return rv;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	long rv;

	SCardLockThread();
	rv = SCardReleaseContextTH(hContext);
	SCardUnlockThread();

	return rv;
}


static LONG SCardListReadersTH(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	static int first_time = 1;
	int i = 0;
	static DWORD dwReadersLen = 0;
	LONG retIndice = 0;
	char *tempPtr;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

/* Check for NULL parameters */
	if (pcchReaders == NULL)
	{
		return SCARD_E_INVALID_PARAMETER;
	}
	/*Check for Context validity */
	retIndice = SCardGetContextIndice(hContext);
	if (0 > retIndice)
		return SCARD_E_INVALID_HANDLE;

	/*Calculate the the buffer length reuired only once.
	 */
	if (first_time)
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if (NULL != psReaderMap[i].ReaderName)
				dwReadersLen += strlen(psReaderMap[i].ReaderName) + 1;
		}
		dwReadersLen++;
		first_time = 0;
	}
	/*There are no readers available */
	if (1 >= dwReadersLen)
		return SCARD_E_READER_UNAVAILABLE;

	if (mszReaders == NULL)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	}
	else if (*pcchReaders == 0)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_S_SUCCESS;
	}
	else if (*pcchReaders < dwReadersLen)
	{
		*pcchReaders = dwReadersLen;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	tempPtr = mszReaders;
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (NULL != psReaderMap[i].ReaderName)
		{
			memcpy(tempPtr, psReaderMap[i].ReaderName,
				strlen(psReaderMap[i].ReaderName) + 1);
			tempPtr += (strlen(psReaderMap[i].ReaderName) + 1);
		}
	}
	/*the extra NULL character as per the PCSC specs. */
	tempPtr[0] = '\0';
	*pcchReaders = dwReadersLen;

	return SCARD_S_SUCCESS;
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

/* by najam */


static LONG SCardConnectTH(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	/* Zero out everything   */
	rv = 0;

	/* Check for NULL parameters */
	if (phCard == NULL || pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phCard = 0;

	/* Make sure this context has been opened */
	if (SCardGetContextIndice(hContext) == -1)
		return SCARD_E_INVALID_HANDLE;

	if (szReader == NULL)
		return SCARD_E_UNKNOWN_READER;

	/* Check for uninitialized strings */
	if (strlen(szReader) > MAX_READERNAME)
		return SCARD_E_INVALID_VALUE;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{
		return SCARD_E_INVALID_VALUE;
	}

	if ((SCARD_SHARE_SHARED != dwShareMode) &&
		(SCARD_SHARE_EXCLUSIVE != dwShareMode) &&
		(SCARD_SHARE_DIRECT != dwShareMode))
	{
		return SCARD_E_INVALID_VALUE;
	}
	/* TODO Which Protocols have to be supported */
	/* Ignoring protocols for now */
	/* Make sure this handle has been opened */

	rv = getNewHandle(hContext, szReader, phCard, dwShareMode);

	if (SCARD_S_SUCCESS != rv)
		return rv;

	*pdwActiveProtocol = SCARD_PROTOCOL_T0;
	return SCARD_S_SUCCESS;
}

LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode,
	DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	long rv;

	SCardLockThread();
	rv = SCardConnectTH(hContext, szReader, dwShareMode,
		dwPreferredProtocols, phCard, pdwActiveProtocol);
	SCardUnlockThread();
	return rv;
}

static LONG SCardDisconnectTH(SCARDHANDLE hCard, DWORD dwDisposition)
{
	long rv;
	LONG retIndice = 0;

	SCF_Status_t status;

	/*check ocfserver availibility */
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{
		return SCARD_E_INVALID_VALUE;
	}
	/*CHECK HANDLE VALIDITY */
	retIndice = SCardGetHandleIndice(hCard);
	if ((retIndice == -1) || (NULL == psChannelMap[retIndice].SCF_hCard))
		return SCARD_E_INVALID_HANDLE;

	/* TODO Take Care of the Disposition...  */
	/* Resetting the card for SCARD_RESET_CARD | 
	   SCARD_UNPOWER_CARD | SCARD_EJECT_CARD */
	if (SCARD_LEAVE_CARD != dwDisposition)
	{
		/*must acquire the lock to reset card */
		status = SCF_Card_lock(psChannelMap[retIndice].SCF_hCard, 0);
		if ((SCF_STATUS_SUCCESS == status)
			|| (SCF_STATUS_DOUBLELOCK == status))
		{
			status = SCF_Card_reset(psChannelMap[retIndice].SCF_hCard);
			SCF_Card_unlock(psChannelMap[retIndice].SCF_hCard);
			/*a usleep here will allow the RESET_EVENT to be reported and 
			   the Maps to be updated */
			SYS_USleep(10);
		}
	}

	rv = SCardRemoveHandle(hCard);

	return rv;
}

static LONG SCardReconnectTH(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	SCARDCONTEXT hContext;
	LPSTR ReaderName;
	SCARDHANDLE tempHandle;
	LONG rv;

	int retIndice = 0;
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;
	if (pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;

	if (dwInitialization != SCARD_LEAVE_CARD &&
		dwInitialization != SCARD_RESET_CARD &&
		dwInitialization != SCARD_UNPOWER_CARD &&
		dwInitialization != SCARD_EJECT_CARD)
	{
		return SCARD_E_INVALID_VALUE;
	}

	retIndice = SCardGetHandleIndice(hCard);

	if (-1 == retIndice)
		return SCARD_E_INVALID_HANDLE;

	hContext = psChannelMap[retIndice].hContext;
	ReaderName = psReaderMap[psChannelMap[retIndice].ReaderIndice].ReaderName;

	SCardDisconnectTH(hCard, dwInitialization);

	/* get a new handle */
	rv = SCardConnectTH(hContext, ReaderName, dwShareMode,
		dwPreferredProtocols, &tempHandle, pdwActiveProtocol);
	if (SCARD_S_SUCCESS != rv)
		return rv;

	retIndice = SCardGetHandleIndice(tempHandle);
	if (-1 == retIndice)
		return SCARD_E_NO_MEMORY;

	/*set PCSC hCard to old Handle */
	SCardEventLock();
	psChannelMap[retIndice].PCSC_hCard = hCard;
	SCardEventUnlock();

	return SCARD_S_SUCCESS;
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

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	long rv;

	SCardLockThread();
	rv = SCardDisconnectTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;
}


LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;

	SCF_Card_t SCF_hCard;
	SCF_Status_t status;
	/* Zero out everything   */
	rv = 0;

	SCardLockThread();
	if (SCARD_S_SUCCESS != isOCFServerRunning())
	{
		SCardUnlockThread();
		return SCARD_E_NO_SERVICE;
	}
	rv = getCardForHandle(hCard, &SCF_hCard);
	if (SCARD_S_SUCCESS != rv)
	{
		SCardUnlockThread();
		return rv;
	}
	SCardUnlockThread();

	status = SCF_Card_lock(SCF_hCard, SCF_TIMEOUT_MAX);

	if (SCF_STATUS_DOUBLELOCK == status)
		return SCARD_S_SUCCESS;

	rv = ConvertStatus(status);

	return rv;
}

static LONG SCardEndTransactionTH(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	LONG retIndice = 0;
	SCF_Card_t SCF_hCard;
	SCF_Status_t status;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;
	/* Zero out everything   */
	rv = 0;
	if (dwDisposition != SCARD_LEAVE_CARD &&
		dwDisposition != SCARD_RESET_CARD &&
		dwDisposition != SCARD_UNPOWER_CARD &&
		dwDisposition != SCARD_EJECT_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}
	retIndice = SCardGetHandleIndice(hCard);
	if (retIndice == -1)
		return SCARD_E_INVALID_HANDLE;

	rv = getCardForHandle(hCard, &SCF_hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/* TODO Take Care of the Disposition... */
	if (SCARD_LEAVE_CARD != dwDisposition)
	{
		status = SCF_Card_reset(psChannelMap[retIndice].SCF_hCard);
		if (SCF_STATUS_SUCCESS == status)
		{
			/* reset the isReset for this card */
			SYS_USleep(10);
			SCardEventLock();
			psChannelMap[retIndice].isReset = 0;
			SCardEventUnlock();
		}
	}

	status = SCF_Card_unlock(SCF_hCard);

	return ConvertStatus(status);
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	long rv;

	SCardLockThread();
	rv = SCardEndTransactionTH(hCard, dwDisposition);
	SCardUnlockThread();
	return rv;
}

static LONG SCardCancelTransactionTH(SCARDHANDLE hCard)
{
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	/* TODO */
	return SCARD_S_SUCCESS;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	long rv;

	SCardLockThread();
	rv = SCardCancelTransactionTH(hCard);
	SCardUnlockThread();
	return rv;
}

static LONG SCardStatusTH(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG retIndice, rv;
	int i;
	DWORD dwReaderLen;
	SCF_Card_t SCF_hCard;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;
	/* Zero out everything   */
	retIndice = 0;
	dwReaderLen = 0;
	rv = 0;
	i = 0;
	/* Check for NULL parameters */

	if (pcchReaderLen == NULL || pdwState == NULL ||
		pdwProtocol == NULL || pcbAtrLen == NULL)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	retIndice = SCardGetHandleIndice(hCard);

	rv = getCardForHandle(hCard, &SCF_hCard);
	if (SCARD_S_SUCCESS != rv)
		return rv;

	dwReaderLen =
		strlen(psReaderMap[psChannelMap[retIndice].ReaderIndice].ReaderName);

	if (mszReaderNames == NULL)
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
	strcpy(mszReaderNames,
		psReaderMap[psChannelMap[retIndice].ReaderIndice].ReaderName);
	*pdwProtocol = SCARD_PROTOCOL_T0;

	SCardEventLock();
	if (!(psReaderMap[psChannelMap[retIndice].ReaderIndice].
			dwCurrentState & SCARD_STATE_PRESENT))
	{
		*pdwState = SCARD_ABSENT;
		SCardEventUnlock();
		return SCARD_S_SUCCESS;
	}

	*pdwState = SCARD_NEGOTIABLE | SCARD_POWERED | SCARD_PRESENT;
	rv = PCSC_SCF_getATR(SCF_hCard, pbAtr, pcbAtrLen);
	if (SCARD_S_SUCCESS == rv)
	{
		/*referesh the Atr in the reader Map */
		psReaderMap[psChannelMap[retIndice].ReaderIndice].dwAtrLength =
			*pcbAtrLen;
		memcpy(psReaderMap[psChannelMap[retIndice].ReaderIndice].bAtr, pbAtr,
			*pcbAtrLen);
	}

	SCardEventUnlock();
	return SCARD_S_SUCCESS;
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

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{

	LONG rv, retIndice, readerIndice;
	PSCARD_READERSTATE_A currReader;
	PREADER_STATE rContext;
	LPSTR lpcReaderName;
	DWORD dwTime;
	DWORD dwState;
	DWORD dwBreakFlag;
	int i, j;

	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	/* Zero out everything */
	rv = 0;
	rContext = 0;
	lpcReaderName = 0;
	dwTime = 0;
	j = 0;
	dwState = 0;
	i = 0;
	currReader = 0;
	retIndice = 0;
	readerIndice = 0;
	dwBreakFlag = 0;

	if (rgReaderStates == NULL && cReaders > 0)
		return SCARD_E_INVALID_PARAMETER;

	if (cReaders < 0)
		return SCARD_E_INVALID_VALUE;

	/* change by najam */
	SCardLockThread();
	retIndice = SCardGetContextIndice(hContext);
	/* change by najam */
	SCardUnlockThread();
	if (retIndice == -1)
		return SCARD_E_INVALID_HANDLE;

	/* Application is waiting for a reader -
	   return the first available reader 
	 */
	if (cReaders == 0)
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if (psReaderMap[i].ReaderName)
				return SCARD_S_SUCCESS;
		}
		return SCARD_E_READER_UNAVAILABLE;
	}
	else if (cReaders > PCSCLITE_MAX_READERS_CONTEXTS)
	{
		return SCARD_E_INVALID_VALUE;
	}
	/* Check the integrity of the reader states structures */
	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		if (currReader->szReader == NULL)
		{
			return SCARD_E_INVALID_VALUE;
		}
	}
	/* End of search for readers */

	/* Clear the event state for all readers */
	for (j = 0; j < cReaders; j++)
	{
		currReader = &rgReaderStates[j];
		currReader->dwEventState = 0;
	}

	/* Now is where we start our event checking loop */

	psContextMap[retIndice].contextBlockStatus = BLOCK_STATUS_BLOCKING;
	j = 0;

	do
	{
		SYS_USleep(10);
		if (SCARD_S_SUCCESS != isOCFServerRunning())
			return SCARD_E_NO_SERVICE;

		currReader = &rgReaderStates[j];

	/************ Look for IGNORED readers ****************************/

		if (currReader->dwCurrentState & SCARD_STATE_IGNORE)
		{
			currReader->dwEventState = SCARD_STATE_IGNORE;
		}
		else
		{
	  /************ Looks for correct readernames *********************/

			lpcReaderName = (char *) currReader->szReader;

			readerIndice = SCardGetReaderIndice(lpcReaderName);
			/* The requested reader name is not recognized */
			if (0 > readerIndice)
			{
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState = SCARD_STATE_UNKNOWN;
				}
				else
				{
					currReader->dwEventState =
						SCARD_STATE_UNKNOWN | SCARD_STATE_CHANGED;
					/* Spec says use SCARD_STATE_IGNORE but a removed USB reader
					   with eventState fed into currentState will be ignored forever */
					dwBreakFlag = 1;
				}
			}
			else
			{
				/* The reader has come back after being away */
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					dwBreakFlag = 1;
				}

	/*****************************************************************/
				SCardEventLock();
				/* Now we check all the Reader States */
				dwState = psReaderMap[readerIndice].dwCurrentState;

	/*********** Check if the reader is in the correct state ********/
				if (dwState & SCARD_STATE_UNKNOWN)
				{
					/* App thinks reader is in bad state and it is */
					if (currReader->dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState = SCARD_STATE_UNAVAILABLE;
					}
					else
					{
						/* App thinks reader is in good state and it is not */
						currReader->dwEventState = SCARD_STATE_CHANGED |
							SCARD_STATE_UNAVAILABLE;
						dwBreakFlag = 1;
					}
				}
				else
				{
					/* App thinks reader in bad state but it is not */
					if (currReader->dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

	/********** Check for card presence in the reader **************/

				if (dwState & SCARD_STATE_PRESENT)
				{
					currReader->cbAtr = psReaderMap[readerIndice].dwAtrLength;
					memcpy(currReader->rgbAtr, psReaderMap[readerIndice].bAtr,
						currReader->cbAtr);
				}
				else
				{
					currReader->cbAtr = 0;
				}
				/* Card is now absent                   */
				if (dwState & SCARD_STATE_EMPTY)
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
					/* After present the rest are assumed */
					if (currReader->dwCurrentState & SCARD_STATE_PRESENT ||
						currReader->dwCurrentState & SCARD_STATE_ATRMATCH ||
						currReader->dwCurrentState & SCARD_STATE_EXCLUSIVE ||
						currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
					/* Card is now present              */
				}
				else if (dwState & SCARD_STATE_PRESENT)
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
					/* TODO */
					if (0 && dwState & SCARD_SWALLOWED)
					{
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_MUTE;
						}
						else
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
					}
					else
					{
						/* App thinks card is mute but it is not */
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}

				if (-1 == psReaderMap[readerIndice].SharedRefCount)
				{
					currReader->dwEventState |= SCARD_STATE_EXCLUSIVE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					if (!currReader->dwCurrentState & SCARD_STATE_EXCLUSIVE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}
				else if (psReaderMap[readerIndice].SharedRefCount >= 1)
				{
					/* A card must be inserted for it to be INUSE */
					if (dwState & SCARD_STATE_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_INUSE;
						currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;
						if (!currReader->dwCurrentState & SCARD_STATE_INUSE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}
				SCardEventUnlock();
				if (currReader->dwCurrentState == SCARD_STATE_UNAWARE)
				{
					/* Break out of the while .. loop and return status 
					   once all the status's for all readers is met */
					dwBreakFlag = 1;
				}
				SYS_USleep(PCSCLITE_STATUS_WAIT);

			}					/* End of SCARD_STATE_UNKNOWN */

		}						/* End of SCARD_STATE_IGNORE */

		/* Counter and resetter */
		j = j + 1;
		if (j == cReaders)
			j = 0;

		if (dwTimeout != INFINITE && dwTimeout != 0)
		{
			dwTime += PCSCLITE_STATUS_WAIT;

			/* If time is greater than timeout and all readers have been 
			   checked
			 */
			if ((dwTime >= (dwTimeout * 1000)) && (j == 0))
			{
				return SCARD_E_TIMEOUT;
			}
		}

		/* Declare all the break conditions */
		/* TODO think about this */
		if (psContextMap[retIndice].contextBlockStatus == BLOCK_STATUS_RESUME)
			break;

		/* Break if UNAWARE is set and all readers have been checked */
		if ((dwBreakFlag == 1) && (j == 0))
			break;

		/*
		 * Solve the problem of never exiting the loop when a smartcard is
		 * already inserted in the reader, thus blocking the application
		 * (patch proposed by Serge Koganovitsch)
		 */
		if ((dwTimeout == 0) && (j == 0))
			break;

	}
	while (1);					/* end of do */

	if (psContextMap[retIndice].contextBlockStatus == BLOCK_STATUS_RESUME)
	{
		return SCARD_E_CANCELLED;
	}

	return SCARD_S_SUCCESS;
}


LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer,
	DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	/* TODO */
	return SCARD_S_SUCCESS;
}

static LONG SCardTransmitTH(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	BYTE Buffer[MAX_BUFFER_SIZE];
	LONG rv = 0;
	SCF_Card_t SCF_hCard;
	SCF_Status_t status;
	LONG retIndice;
	LONG localRecvLen = MAX_BUFFER_SIZE;
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;
	if (pbSendBuffer == NULL || pbRecvBuffer == NULL ||
		pcbRecvLength == NULL || pioSendPci == NULL)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	rv = getCardForHandle(hCard, &SCF_hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if ((cbSendLength > MAX_BUFFER_SIZE) || (*pcbRecvLength < 2))
		return SCARD_E_INSUFFICIENT_BUFFER;

	/* TODO which protocols to support */
	/* if(pioSendPci && pioSendPci->dwProtocol) { */
	retIndice = SCardGetHandleIndice(hCard);
	if ((pbSendBuffer[1] == 0xC0) &&
		psTransmitMap[retIndice].isResponseCached)
	{
		if (*pcbRecvLength < psTransmitMap[retIndice].bufferLength)
		{
			*pcbRecvLength = psTransmitMap[retIndice].bufferLength;
			return SCARD_E_INSUFFICIENT_BUFFER;
		}
		*pcbRecvLength = psTransmitMap[retIndice].bufferLength;
		memcpy(pbRecvBuffer, psTransmitMap[retIndice].Buffer,
			psTransmitMap[retIndice].bufferLength);
		if (pioRecvPci && pioSendPci)
			pioRecvPci->dwProtocol = pioSendPci->dwProtocol;
		return SCARD_S_SUCCESS;
	}
	else
	{
		psTransmitMap[retIndice].isResponseCached = 0;
	}

	status = SCF_Card_exchangeAPDU(SCF_hCard,
		(const uint8_t *) pbSendBuffer, (size_t) cbSendLength,
		(uint8_t *) Buffer, (size_t *) & localRecvLen);
	if ((cbSendLength > 5) && (localRecvLen > 2))
	{
		if (SCF_STATUS_SUCCESS == status)
		{
			*pcbRecvLength = 2;
			pbRecvBuffer[0] = 0x61;
			pbRecvBuffer[1] = localRecvLen - 2;
			psTransmitMap[retIndice].isResponseCached = TRUE;
			psTransmitMap[retIndice].bufferLength = localRecvLen;
			memcpy(psTransmitMap[retIndice].Buffer, Buffer,
				psTransmitMap[retIndice].bufferLength);
			if (pioRecvPci && pioSendPci)
				pioRecvPci->dwProtocol = pioSendPci->dwProtocol;
			return SCARD_S_SUCCESS;
		}
	}
	else
	{
		if (SCF_STATUS_SUCCESS == status)
		{
			if (*pcbRecvLength < localRecvLen)
			{
				*pcbRecvLength = localRecvLen;
				return SCARD_E_INSUFFICIENT_BUFFER;
			}
			*pcbRecvLength = localRecvLen;
			memcpy(pbRecvBuffer, Buffer, *pcbRecvLength);
		}
	}

	/* TODO fill the received Pci ... */
	/* For now  just filling the send pci protocol. */
	if (pioRecvPci && pioSendPci)
		pioRecvPci->dwProtocol = pioSendPci->dwProtocol;

	rv = ConvertStatus(status);
	return rv;
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	long rv;

	SCardLockThread();
	rv = SCardTransmitTH(hCard, pioSendPci, pbSendBuffer, cbSendLength,
		pioRecvPci, pbRecvBuffer, pcbRecvLength);
	SCardUnlockThread();

	return rv;
}


static LONG SCardListReaderGroupsTH(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{
	LONG rv = SCARD_S_SUCCESS;
	const char ReaderGroup[] = "SCard$DefaultReaders";
	const int dwGroups = strlen(ReaderGroup) + 2;
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;
	/* Make sure this context has been opened */
	if (SCardGetContextIndice(hContext) == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}
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

LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{
	long rv;

	SCardLockThread();
	rv = SCardListReaderGroupsTH(hContext, mszGroups, pcchGroups);
	SCardUnlockThread();

	return rv;
}

static LONG SCardCancelTH(SCARDCONTEXT hContext)
{
	LONG hContextIndice;
	if (SCARD_S_SUCCESS != isOCFServerRunning())
		return SCARD_E_NO_SERVICE;

	hContextIndice = SCardGetContextIndice(hContext);

	if (hContextIndice == -1)
		return SCARD_E_INVALID_HANDLE;

	/* Set the block status for this Context so blocking calls will complete */
	psContextMap[hContextIndice].contextBlockStatus = BLOCK_STATUS_RESUME;

	return SCARD_S_SUCCESS;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{
	long rv;

	SCardLockThread();
	rv = SCardCancelTH(hContext);
	SCardUnlockThread();

	return rv;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
	LPDWORD pcbAttrLen)
{
	return SCARD_E_NOT_TRANSACTED;
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
	DWORD cbAttrLen)
{
	return SCARD_E_NOT_TRANSACTED;
}

static LONG SCardGetHandleIndice(SCARDHANDLE hCard)
{
	int i = 0;
	static int LastIndex = 0;

	if (hCard == 0)
		return -1;
	if (psChannelMap[LastIndex].PCSC_hCard == hCard)
		return LastIndex;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (hCard == psChannelMap[i].PCSC_hCard)
			return i;
	}

	return -1;
}
static LONG SCardGetReaderIndice(LPCSTR ReaderName)
{
	int i = 0;

	if (NULL == ReaderName)
		return -1;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((NULL != psReaderMap[i].ReaderName) &&
			(strncmp(psReaderMap[i].ReaderName, ReaderName,
					strlen(psReaderMap[i].ReaderName)) == 0))
		{
			return i;
		}
	}

	return -1;
}

static LONG SCardAddHandle(SCARDHANDLE PCSC_hCard, SCARDCONTEXT hContext,
	SCF_Session_t hSession, SCF_Terminal_t hTerminal,
	SCF_Card_t SCF_hCard, int ReaderIndice, DWORD dwShareMode)
{
	int i = 0;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psChannelMap[i].PCSC_hCard == 0)
		{
			psChannelMap[i].PCSC_hCard = PCSC_hCard;
			psChannelMap[i].hContext = hContext;
			psChannelMap[i].hSession = hSession;
			psChannelMap[i].hTerminal = hTerminal;
			psChannelMap[i].SCF_hCard = SCF_hCard;
			psChannelMap[i].ReaderIndice = ReaderIndice;
			SCardEventLock();
			if (SCARD_SHARE_EXCLUSIVE == dwShareMode)
			{
				psChannelMap[i].haveLock = TRUE;
				psReaderMap[ReaderIndice].SharedRefCount = -1;
			}
			else
			{
				psReaderMap[ReaderIndice].SharedRefCount++;
				psReaderMap[ReaderIndice].dwCurrentState |= SCARD_STATE_INUSE;
			}
			PCSC_SCF_getATR(SCF_hCard, psReaderMap[ReaderIndice].bAtr,
				&psReaderMap[ReaderIndice].dwAtrLength);
			SCardEventUnlock();
			return SCARD_S_SUCCESS;
		}
	}
	return SCARD_E_NO_MEMORY;
}

static LONG PCSC_SCF_getATR(SCF_Card_t hCard, LPBYTE pcbAtr,
	LPDWORD pcbAtrLen)
{
	SCF_Status_t status;

	struct SCF_BinaryData_t *pAtr;

	status = SCF_Card_getInfo(hCard, "atr", &pAtr);
	if (SCF_STATUS_SUCCESS != status)
		return SCARD_F_COMM_ERROR;

	if ((NULL == pcbAtr) || (NULL == pcbAtrLen) ||
		(MAX_ATR_SIZE < pAtr->length))
	{
		if (NULL != pcbAtrLen)
			*pcbAtrLen = pAtr->length;
		SCF_Card_freeInfo(hCard, pAtr);
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	*pcbAtrLen = pAtr->length;

	memcpy(pcbAtr, pAtr->data, pAtr->length);

	SCF_Card_freeInfo(hCard, pAtr);
	return SCARD_S_SUCCESS;
}

static LONG SCardRemoveHandle(SCARDHANDLE hCard)
{
	LONG retIndice = 0;

	retIndice = SCardGetHandleIndice(hCard);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}
	SCardEventLock();
	SCF_Session_close(psChannelMap[retIndice].hSession);
	psChannelMap[retIndice].PCSC_hCard = 0;
	psChannelMap[retIndice].hContext = 0;
	psChannelMap[retIndice].hSession = NULL;
	psChannelMap[retIndice].hTerminal = NULL;
	psChannelMap[retIndice].SCF_hCard = NULL;
	psChannelMap[retIndice].isReset = 0;
	if (psChannelMap[retIndice].haveLock)
	{
		psChannelMap[retIndice].haveLock = FALSE;
		psReaderMap[psChannelMap[retIndice].ReaderIndice].SharedRefCount = 0;
	}
	else
	{
		psReaderMap[psChannelMap[retIndice].ReaderIndice].SharedRefCount--;
		if (0 >=
			psReaderMap[psChannelMap[retIndice].ReaderIndice].SharedRefCount)
		{
			psReaderMap[psChannelMap[retIndice].ReaderIndice].SharedRefCount =
				0;
			psReaderMap[psChannelMap[retIndice].ReaderIndice].
				dwCurrentState &=
				(~(SCARD_STATE_EXCLUSIVE | SCARD_STATE_INUSE));
		}
	}

	psChannelMap[retIndice].ReaderIndice = 0;
	SCardEventUnlock();
	return SCARD_S_SUCCESS;
}


static LONG getCardForHandle(SCARDHANDLE PCSC_hCard, SCF_Card_t * SCF_hCard)
{
	int retIndice = 0;

	retIndice = SCardGetHandleIndice(PCSC_hCard);
	if (0 > retIndice)
		return SCARD_E_INVALID_HANDLE;

	*SCF_hCard = psChannelMap[retIndice].SCF_hCard;
	if (NULL == *SCF_hCard)
		return SCARD_E_INVALID_HANDLE;
	SCardEventLock();
	if (psChannelMap[retIndice].isReset)
	{
		SCardEventUnlock();
		return SCARD_W_RESET_CARD;
	}
	SCardEventUnlock();

	return SCARD_S_SUCCESS;

}

static LONG getNewHandle(SCARDCONTEXT hContext, LPCSTR szReader,
	SCARDHANDLE * phCard, DWORD dwShareMode)
{
	long rv = 0, ReaderIndice;
	SCF_Status_t status;
	SCF_Session_t hSession;
	SCF_Terminal_t hTerminal;
	SCF_Card_t SCF_hCard;

	ReaderIndice = SCardGetReaderIndice(szReader);
	if (-1 == ReaderIndice)
		return SCARD_E_UNKNOWN_READER;

	SCardEventLock();
	if ((psReaderMap[ReaderIndice].SharedRefCount == -1) ||
		((SCARD_SHARE_EXCLUSIVE == dwShareMode) &&
			psReaderMap[ReaderIndice].SharedRefCount))
	{
		SCardEventUnlock();
		return SCARD_E_SHARING_VIOLATION;
	}
	SCardEventUnlock();

	status = SCF_Session_getSession(&hSession);
	if (SCF_STATUS_SUCCESS != status)
	{
		return ConvertStatus(status);
	}
	status = SCF_Session_getTerminal(hSession, szReader, &hTerminal);
	if (SCF_STATUS_SUCCESS != status)
	{
		SCF_Session_close(hSession);
		return ConvertStatus(status);
	}
	status = SCF_Terminal_getCard(hTerminal, &SCF_hCard);
	if (SCF_STATUS_SUCCESS != status)
	{
		SCF_Session_close(hSession);
		return ConvertStatus(status);
	}

	if (SCARD_SHARE_EXCLUSIVE == dwShareMode)
	{
		status = SCF_Card_lock(SCF_hCard, 0);
		if (status != SCF_STATUS_SUCCESS)
		{
			SCF_Session_close(hSession);
			return SCARD_E_SHARING_VIOLATION;
		}
	}

	while (1)
	{
		*phCard = (PCSCLITE_SVC_IDENTITY + SYS_RandomInt(1, 65535));
		if (SCardGetHandleIndice(*phCard) == -1)
			break;
	}
	rv = SCardAddHandle(*phCard, hContext, hSession,
		hTerminal, SCF_hCard, ReaderIndice, dwShareMode);
	if (SCARD_S_SUCCESS != rv)
	{
		SCF_Session_close(hSession);
		return rv;
	}

	return SCARD_S_SUCCESS;
}



/*
  Function Managing Terminals for Sessions
*/


/*
  Functions for managing instances of SCardEstablishContext
  These functions keep track of Context handles and associate
  the blocking variable contextBlockStatus to an hContext
*/

static LONG getNewContext(SCARDCONTEXT * phContext)
{
	LONG rv;
	SCF_Session_t hSession = NULL;
	SCF_Status_t status;

	status = SCF_Session_getSession(&hSession);
	if (status != SCF_STATUS_SUCCESS)
		return SCARD_E_NO_SERVICE;

	while (1)
	{
		*phContext = (PCSCLITE_SVC_IDENTITY + SYS_RandomInt(1, 65535));
		if (-1 == SCardGetContextIndice(*phContext))
			break;
	}

	rv = SCardAddContext(*phContext, hSession);
	if (SCARD_S_SUCCESS != rv)
	{
		SCF_Session_close(hSession);
	}

	return rv;
}

static LONG SCardAddContext(SCARDCONTEXT hContext, SCF_Session_t hSession)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == 0)
		{
			psContextMap[i].hContext = hContext;
			psContextMap[i].hSession = hSession;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{
	int i;

	/* Find this context and return it's spot in the array */
	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
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
	int i = 0;
	retIndice = 0;

	retIndice = SCardGetContextIndice(hContext);

	if (retIndice == -1)
	{
		return SCARD_E_INVALID_HANDLE;
	}
	else
	{
		/* Free all handles for this context. */
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			if (psChannelMap[i].hContext == hContext)
			{
				SCardRemoveHandle(psChannelMap[i].PCSC_hCard);
			}
		}
		SCF_Session_close(psContextMap[retIndice].hSession);
		psContextMap[retIndice].hContext = 0;
		psContextMap[retIndice].hSession = NULL;
		psContextMap[retIndice].contextBlockStatus = BLOCK_STATUS_RESUME;
	}

	return SCARD_S_SUCCESS;
}

static SCF_Session_t getSessionForContext(SCARDCONTEXT hContext)
{
	LONG retIndice;
	retIndice = 0;

	retIndice = SCardGetContextIndice(hContext);

	if (retIndice == -1)
		return NULL;

	return (psContextMap[retIndice].hSession);
}


/*  This function locks a mutex so another thread
    must wait to use this function
*/

LONG SCardLockThread(void)
{
	return SYS_MutexLock(&clientMutex);
}

LONG SCardEventLock(void)
{
	return SYS_MutexLock(&EventMutex);
}

/*  This function unlocks a mutex so another thread
    may use the client library 
*/

LONG SCardUnlockThread(void)
{
	return SYS_MutexUnLock(&clientMutex);
}

LONG SCardEventUnlock(void)
{
	return SYS_MutexUnLock(&EventMutex);
}

static LONG isActiveContextPresent(void) 
{
	long fActiveContext = FALSE;
	int i;

	for (i=0; i<PCSCLITE_MAX_APPLICATION_CONTEXTS; i++) 
	{
		if (psContextMap[i].hContext != 0) 
		{
			fActiveContext = TRUE;
			break;
		}
	}
	return fActiveContext;
}

static void EventCallback(SCF_Event_t eventType, SCF_Terminal_t hTerm,
	void *cbdata)
{
	static char bInitialized = 0;
	int i = 0;
	int ReaderIndice = 0;
	SCF_Card_t hCard;

	SCF_Status_t status;

#if 0
	struct _psReaderMap *readerMap;
	readerMap = (struct _psReaderMap *) cbdata;
#endif

	ReaderIndice = (int) cbdata;
	SCardEventLock();
	switch (eventType)
	{
	case SCF_EVENT_CARDINSERTED:
	case SCF_EVENT_CARDPRESENT:
#if 0
		printf("card present dwState = %x\n",
			psReaderMap[ReaderIndice].dwCurrentState);
#endif
		psReaderMap[ReaderIndice].dwCurrentState &=
			(~(SCARD_STATE_UNKNOWN | SCARD_STATE_UNAVAILABLE |
				SCARD_STATE_EMPTY));
		psReaderMap[ReaderIndice].dwCurrentState |= SCARD_STATE_PRESENT;
#if 0
		printf("card present post dwState = %x\n",
			psReaderMap[ReaderIndice].dwCurrentState);
#endif
		/* TODO get the ATR... filled */
		status = SCF_Terminal_getCard(psReaderMap[ReaderIndice].hTerminal,
			&hCard);
		if (SCF_STATUS_SUCCESS == status)
		{
#if 0
			printf("Setting ATR...\n");
#endif
			PCSC_SCF_getATR(hCard, psReaderMap[ReaderIndice].bAtr,
				&psReaderMap[ReaderIndice].dwAtrLength);
#if 0
			printf("Atrlen = %d\n", psReaderMap[ReaderIndice].dwAtrLength);
#endif
		}
		SCF_Card_close(hCard);
		break;
	case SCF_EVENT_CARDREMOVED:
	case SCF_EVENT_CARDABSENT:
#if 0
		printf("card absent dwState = %x\n",
			psReaderMap[ReaderIndice].dwCurrentState);
#endif
		psReaderMap[ReaderIndice].dwCurrentState &= (~(SCARD_STATE_PRESENT |
				SCARD_STATE_EXCLUSIVE |
				SCARD_STATE_INUSE |
				SCARD_STATE_MUTE | SCARD_STATE_UNAVAILABLE));
		psReaderMap[ReaderIndice].dwCurrentState |= SCARD_STATE_EMPTY;
		psReaderMap[ReaderIndice].SharedRefCount = 0;
		psReaderMap[ReaderIndice].dwAtrLength = 0;
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			if ((0 != psChannelMap[i].PCSC_hCard) &&
				(psChannelMap[i].ReaderIndice == ReaderIndice))
			{
				psChannelMap[i].haveLock = FALSE;
			}
		}
#if 0
		printf("card absent dwState = %x\n",
			psReaderMap[ReaderIndice].dwCurrentState);
#endif
		break;
	case SCF_EVENT_TERMINALCLOSED:
		/* TODO .... */
		break;
	case SCF_EVENT_CARDRESET:
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			if ((0 != psChannelMap[i].PCSC_hCard) &&
				(psChannelMap[i].ReaderIndice == ReaderIndice))
			{
				psChannelMap[i].isReset = TRUE;
			}
		}
		break;
	default:
		break;
	}							/* switch */
	/*
	 * The OCF Server always calls the registered callback function 
	 * immediately after the callback function is registered,
	 * but always with the state of SCF_EVENT_CARDABSENT even if 
	 * there is a card present in the reader, the OCF server then 
	 * calls the callback the second time with the correct state,
	 * that is the reason for the check if (bInitialized == 2).
	 * If there is no card present in the reader the PCSC_SCF_Initialize's 
	 * pthread_cond_timedwait times out after 2 secs.
     */
	if (bInitialized < 2)
	{
		bInitialized++;
		if (2 == bInitialized)
			pthread_cond_signal(&EventCondition);
	} 

	SCardEventUnlock();
}


static LONG isOCFServerRunning(void)
{
	static int isRunning = TRUE;
	SCF_Status_t status;
	SCF_Session_t hSession;

	if (FALSE == isRunning)
		return SCARD_E_NO_SERVICE;

	status = SCF_Session_getSession(&hSession);
	if (SCF_STATUS_SUCCESS != status)
	{
		isRunning = FALSE;
		return SCARD_E_NO_SERVICE;
	}
	SCF_Session_close(hSession);

	return SCARD_S_SUCCESS;
}

static LONG PCSC_SCF_Initialize(void)
{
	SCF_Status_t status;
	char **tList = NULL;
	int i;

	if (PCSC_Initialized)
		return SCARD_S_SUCCESS;
	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		psContextMap[i].hContext = 0;
		psContextMap[i].hSession = 0;
		psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
		psChannelMap[i].PCSC_hCard = 0;
		psChannelMap[i].SCF_hCard = 0;
		psChannelMap[i].hTerminal = 0;
		psChannelMap[i].hContext = 0;
		psChannelMap[i].hSession = 0;
		psChannelMap[i].haveLock = 0;
		psChannelMap[i].isReset = 0;
		psChannelMap[i].ReaderIndice = 0;
		psTransmitMap[i].isResponseCached = 0;
		psTransmitMap[i].bufferLength = 0;
	}
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		psReaderMap[i].ReaderName = NULL;
		psReaderMap[i].hTerminal = 0;
		psReaderMap[i].SharedRefCount = 0;
		psReaderMap[i].dwCurrentState |= SCARD_STATE_UNAVAILABLE;
		psReaderMap[i].dwAtrLength = 0;
		memset(psReaderMap[i].bAtr, 0, MAX_ATR_SIZE);
		psReaderMap[i].lHandle = NULL;
	}

	status = SCF_Session_getSession(&g_hSession);
	if (status != SCF_STATUS_SUCCESS)
		return SCARD_E_NO_SERVICE;
	status = SCF_Session_getInfo(g_hSession, "terminalnames", &tList);
	if (status != SCF_STATUS_SUCCESS)
		return SCARD_E_NO_SERVICE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (NULL == tList[i])
			break;
		psReaderMap[i].ReaderName = strdup(tList[i]);
		status =
			SCF_Session_getTerminal(g_hSession, psReaderMap[i].ReaderName,
			&psReaderMap[i].hTerminal);
		if (status != SCF_STATUS_SUCCESS)
			continue;
		status =
			SCF_Terminal_addEventListener(psReaderMap[i].hTerminal,
			SCF_EVENT_ALL, EventCallback, (void *) i,
			&psReaderMap[i].lHandle);
		if (status != SCF_STATUS_SUCCESS)
		{
			SCF_Terminal_close(psReaderMap[i].hTerminal);
			psReaderMap[i].hTerminal = NULL;
		}
	}
	SCF_Session_freeInfo(g_hSession, tList);

	SYS_MutexLock(&SCFInitMutex);
	/* wait for ocfserver to initialize... or 2 secs whichever is earlier */
	{
		struct timeval currTime;
		struct timespec absTime;

		gettimeofday(&currTime, NULL);

		/* Calculate absolute time to time out */
		absTime.tv_sec = currTime.tv_sec + 2;
		absTime.tv_nsec = currTime.tv_usec*1000;

		pthread_cond_timedwait(&EventCondition, &SCFInitMutex, &absTime);
	}
	SYS_MutexUnLock(&SCFInitMutex);

	PCSC_Initialized = 1;
	return SCARD_S_SUCCESS;
}

static LONG ConvertStatus(SCF_Status_t status)
{
	switch (status)
	{
	case SCF_STATUS_COMMERROR:
		return SCARD_F_COMM_ERROR;
	case SCF_STATUS_FAILED:
		return SCARD_F_INTERNAL_ERROR;
	case SCF_STATUS_BADHANDLE:
		return SCARD_E_INVALID_HANDLE;
	case SCF_STATUS_UNKNOWNPROPERTY:
		return SCARD_F_UNKNOWN_ERROR;
	case SCF_STATUS_BADARGS:
		return SCARD_E_INVALID_VALUE;
	case SCF_STATUS_BADTERMINAL:
		return SCARD_E_READER_UNAVAILABLE;
	case SCF_STATUS_NOCARD:
		return SCARD_E_NO_SMARTCARD;
	case SCF_STATUS_CARDREMOVED:
		return SCARD_W_REMOVED_CARD;
	case SCF_STATUS_TIMEOUT:
		return SCARD_E_TIMEOUT;
#if 0
	case SCF_STATUS_DOUBLELOCK:
		/* TODO */
		break;
#endif
	case SCF_STATUS_CARDLOCKED:
		return SCARD_E_SHARING_VIOLATION;
	case SCF_STATUS_NOSPACE:
		return SCARD_E_NO_MEMORY;
	case SCF_STATUS_SUCCESS:
		return SCARD_S_SUCCESS;
	}
	return SCARD_F_UNKNOWN_ERROR;
}

/*
 * Note that this function is not used
 */
LONG SCardCheckReaderAvailability(LPSTR readerName, LONG errorCode)
{
#if 0
	LONG retIndice;
	int i;

	retIndice = 0;
	i = 0;
	if (errorCode != SCARD_S_SUCCESS)
	{
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			if (strcmp(psChannelMap[i].readerName, readerName) == 0)
			{
				return errorCode;
			}
		}

		return SCARD_E_READER_UNAVAILABLE;

	}
	else
	{
		return SCARD_S_SUCCESS;
	}
#endif
	return 0;
}

/*
 * free resources allocated by the library
 * You _shall_ call this function if you use dlopen/dlclose to load/unload the
 * library. Otherwise you will exhaust the ressources available.
 */
void SCardUnload(void)
{
    int i=0;
#if 0
	if (!isExecuted)
		return;

	SHMClientCloseSession();
	SYS_CloseFile(mapAddr);
	isExecuted = 0;
#endif

    /*
	 *	Cleanup only if PCSC has been initialized and there are no active
	 *	context.  Checking for active context is critical when libpcsclite is
	 *	linked with multiple modules. for eg. an application links with
	 *	pcsclite and a PAM module also links with pcsclite, when the PAM is
	 *	unloaded from memory and if it calls SCardUnload, pcsclite will
	 *	un-initialize even though there are active references from the
	 *	application. Now, why dont we add SCardUnload to the array of functions
	 *	to be called on unload (-zfiniarray=SCUnload), well, that does not seem
	 *	to solve the problem, SCardUnload is called when PAM is unloaded from
	 *	memory having the same impact that PCSC is uninitialized enen though
	 *	there are active references.
     */
	if((!PCSC_Initialized) || isActiveContextPresent()) 
		return;

	for(i=0; i<PCSCLITE_MAX_READERS_CONTEXTS; i++) 
		if (psReaderMap[i].hTerminal)
		{
			SCF_Terminal_removeEventListener(psReaderMap[i].hTerminal,
				psReaderMap[i].lHandle);

			SCF_Terminal_close(psReaderMap[i].hTerminal); 

			if (psReaderMap[i].ReaderName)
				free(psReaderMap[i].ReaderName);
		}   

	SCF_Session_close(g_hSession);
	PCSC_Initialized = 0;
}

/*
 * Note that this function is not used
 */
LONG SCardCheckDaemonAvailability(void)
{
	LONG rv = 1;				/* assume it exists */

	if (rv == 0)
		return SCARD_E_NO_SERVICE;
	else
		return SCARD_S_SUCCESS;
}

