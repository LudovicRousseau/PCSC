/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This keeps track of card insertion/removal events
 * and updates ATR, protocol, and status information.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#include "misc.h"
#include "pcscd.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "eventhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "ifdwrapper.h"
#include "prothandler.h"
#include "strlcpycat.h"

static PREADER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

void EHStatusHandlerThread(PREADER_CONTEXT);

LONG EHInitializeEventStructures(void)
{
	int fd, i, pageSize;

	fd = 0;
	i = 0;
	pageSize = 0;

	SYS_RemoveFile(PCSCLITE_PUBSHM_FILE);

	fd = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDWR | O_CREAT, 00644);
	if (fd < 0)
	{
		Log3(PCSC_LOG_CRITICAL, "Cannot create public shared file %s: %s",
			PCSCLITE_PUBSHM_FILE, strerror(errno));
		exit(1);
	}

	SYS_Chmod(PCSCLITE_PUBSHM_FILE,
		S_IRGRP | S_IREAD | S_IWRITE | S_IROTH);

	pageSize = SYS_GetPageSize();

	/*
	 * Jump to end of file space and allocate zero's
	 */
	SYS_SeekFile(fd, pageSize * PCSCLITE_MAX_READERS_CONTEXTS);
	SYS_WriteFile(fd, "", 1);

	/*
	 * Allocate each reader structure
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		readerStates[i] = (PREADER_STATE)
			SYS_MemoryMap(sizeof(READER_STATE), fd, (i * pageSize));
		if (readerStates[i] == MAP_FAILED)
		{
			Log3(PCSC_LOG_CRITICAL, "Cannot memory map public shared file %s: %s",
				PCSCLITE_PUBSHM_FILE, strerror(errno));
			exit(1);
		}

		/*
		 * Zero out each value in the struct
		 */
		memset((readerStates[i])->readerName, 0, MAX_READERNAME);
		memset((readerStates[i])->cardAtr, 0, MAX_ATR_SIZE);
		(readerStates[i])->readerID = 0;
		(readerStates[i])->readerState = 0;
		(readerStates[i])->readerSharing = 0;
		(readerStates[i])->cardAtrLength = 0;
		(readerStates[i])->cardProtocol = SCARD_PROTOCOL_UNDEFINED;
	}

	return SCARD_S_SUCCESS;
}

LONG EHDestroyEventHandler(PREADER_CONTEXT rContext)
{
	if (NULL == rContext->readerState)
	{
		Log1(PCSC_LOG_ERROR, "Thread never started (reader init failed?)");
		return SCARD_S_SUCCESS;
	}

	if ('\0' == rContext->readerState->readerName[0])
	{
		Log1(PCSC_LOG_INFO, "Thread already stomped.");
		return SCARD_S_SUCCESS;
	}

	/*
	 * Set the thread to 0 to exit thread
	 */
	rContext->dwLockId = 0xFFFF;

	Log1(PCSC_LOG_INFO, "Stomping thread.");

	/* kill the "polling" thread */
	SYS_ThreadCancel(rContext->pthThread);

	/* wait for the thread to finish */
	SYS_ThreadJoin(rContext->pthThread, NULL);

	/*
	 * Zero out the public status struct to allow it to be recycled and
	 * used again
	 */
	memset(rContext->readerState->readerName, 0,
		sizeof(rContext->readerState->readerName));
	memset(rContext->readerState->cardAtr, 0,
		sizeof(rContext->readerState->cardAtr));
	rContext->readerState->readerID = 0;
	rContext->readerState->readerState = 0;
	rContext->readerState->readerSharing = 0;
	rContext->readerState->cardAtrLength = 0;
	rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

	/* Zero the thread */
	rContext->pthThread = 0;

	Log1(PCSC_LOG_INFO, "Thread stomped.");

	return SCARD_S_SUCCESS;
}

LONG EHSpawnEventHandler(PREADER_CONTEXT rContext,
	RESPONSECODE (*card_event)(DWORD))
{
	LONG rv;
	DWORD dwStatus = 0;
	int i;
	UCHAR ucAtr[MAX_ATR_SIZE];
	DWORD dwAtrLen = 0;

	rv = IFDStatusICC(rContext, &dwStatus, ucAtr, &dwAtrLen);
	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "Initial Check Failed on %s", rContext->lpcReader);
		return SCARD_F_UNKNOWN_ERROR;
	}

	/*
	 * Find an empty reader slot and insert the new reader
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((readerStates[i])->readerID == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		return SCARD_F_INTERNAL_ERROR;

	/*
	 * Set all the attributes to this reader
	 */
	rContext->readerState = readerStates[i];
	strlcpy(rContext->readerState->readerName, rContext->lpcReader,
		sizeof(rContext->readerState->readerName));
	memcpy(rContext->readerState->cardAtr, ucAtr, dwAtrLen);
	rContext->readerState->readerID = i + 100;
	rContext->readerState->readerState = dwStatus;
	rContext->readerState->readerSharing = rContext->dwContexts;
	rContext->readerState->cardAtrLength = dwAtrLen;
	rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

	rContext->pthCardEvent = card_event;
	rv = SYS_ThreadCreate(&rContext->pthThread, 0,
		(PCSCLITE_THREAD_FUNCTION( ))EHStatusHandlerThread, (LPVOID) rContext);
	if (rv == 1)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_NO_MEMORY;
}

static void incrementEventCounter(struct pubReaderStatesList *readerState)
{
	int counter;

	counter = (readerState -> readerState >> 16) & 0xFFFF;
	counter++;
	readerState -> readerState = (readerState -> readerState & 0xFFFF)
		+ (counter << 16);
}

void EHStatusHandlerThread(PREADER_CONTEXT rContext)
{
	LONG rv;
	LPCSTR lpcReader;
	DWORD dwStatus, dwReaderSharing;
	DWORD dwCurrentState;
	DWORD dwAtrLen;
	int pageSize;

	/*
	 * Zero out everything
	 */
	dwStatus = 0;
	dwReaderSharing = 0;
	dwCurrentState = 0;

	lpcReader = rContext->lpcReader;

	pageSize = SYS_GetPageSize();

	dwAtrLen = rContext->readerState->cardAtrLength;
	rv = IFDStatusICC(rContext, &dwStatus, rContext->readerState->cardAtr,
		&dwAtrLen);
	rContext->readerState->cardAtrLength = dwAtrLen;

	if (dwStatus & SCARD_PRESENT)
	{
		dwAtrLen = MAX_ATR_SIZE;
		rv = IFDPowerICC(rContext, IFD_POWER_UP,
			rContext->readerState->cardAtr,
			&dwAtrLen);
		rContext->readerState->cardAtrLength = dwAtrLen;

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

		if (rv == IFD_SUCCESS)
		{
			dwStatus |= SCARD_PRESENT;
			dwStatus &= ~SCARD_ABSENT;
			dwStatus |= SCARD_POWERED;
			dwStatus |= SCARD_NEGOTIABLE;
			dwStatus &= ~SCARD_SPECIFIC;
			dwStatus &= ~SCARD_SWALLOWED;
			dwStatus &= ~SCARD_UNKNOWN;

			if (rContext->readerState->cardAtrLength > 0)
			{
				LogXxd(PCSC_LOG_INFO, "Card ATR: ",
					rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
			}
			else
				Log1(PCSC_LOG_INFO, "Card ATR: (NULL)");
		}
		else
		{
			dwStatus |= SCARD_PRESENT;
			dwStatus &= ~SCARD_ABSENT;
			dwStatus |= SCARD_SWALLOWED;
			dwStatus &= ~SCARD_POWERED;
			dwStatus &= ~SCARD_NEGOTIABLE;
			dwStatus &= ~SCARD_SPECIFIC;
			dwStatus &= ~SCARD_UNKNOWN;
			Log3(PCSC_LOG_ERROR, "Error powering up card: %d 0x%04X", rv, rv);
		}

		dwCurrentState = SCARD_PRESENT;
	}
	else
	{
		dwStatus |= SCARD_ABSENT;
		dwStatus &= ~SCARD_PRESENT;
		dwStatus &= ~SCARD_POWERED;
		dwStatus &= ~SCARD_NEGOTIABLE;
		dwStatus &= ~SCARD_SPECIFIC;
		dwStatus &= ~SCARD_SWALLOWED;
		dwStatus &= ~SCARD_UNKNOWN;
		rContext->readerState->cardAtrLength = 0;
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

		dwCurrentState = SCARD_ABSENT;
	}

	/*
	 * Set all the public attributes to this reader
	 */
	rContext->readerState->readerState = dwStatus;
	rContext->readerState->readerSharing = dwReaderSharing =
		rContext->dwContexts;

	SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

	while (1)
	{
		dwStatus = 0;

		dwAtrLen = rContext->readerState->cardAtrLength;
		rv = IFDStatusICC(rContext, &dwStatus,
			rContext->readerState->cardAtr,
			&dwAtrLen);
		rContext->readerState->cardAtrLength = dwAtrLen;

		if (rv != SCARD_S_SUCCESS)
		{
			Log2(PCSC_LOG_ERROR, "Error communicating to: %s", lpcReader);

			/*
			 * Set error status on this reader while errors occur
			 */

			rContext->readerState->readerState &= ~SCARD_ABSENT;
			rContext->readerState->readerState &= ~SCARD_PRESENT;
			rContext->readerState->readerState &= ~SCARD_POWERED;
			rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~SCARD_SPECIFIC;
			rContext->readerState->readerState &= ~SCARD_SWALLOWED;
			rContext->readerState->readerState |= SCARD_UNKNOWN;
			rContext->readerState->cardAtrLength = 0;
			rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

			dwCurrentState = SCARD_UNKNOWN;

			SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

			/*
			 * This code causes race conditions on G4's with USB
			 * insertion
			 */
			/*
			 * dwErrorCount += 1; SYS_Sleep(1);
			 */
			/*
			 * After 10 seconds of errors, try to reinitialize the reader
			 * This sometimes helps bring readers out of *crazy* states.
			 */
			/*
			 * if ( dwErrorCount == 10 ) { RFUnInitializeReader( rContext
			 * ); RFInitializeReader( rContext ); dwErrorCount = 0; }
			 */

			/*
			 * End of race condition code block
			 */
		}

		if (dwStatus & SCARD_ABSENT)
		{
			if (dwCurrentState == SCARD_PRESENT ||
				dwCurrentState == SCARD_UNKNOWN)
			{
				/*
				 * Change the status structure
				 */
				Log2(PCSC_LOG_INFO, "Card Removed From %s", lpcReader);
				/*
				 * Notify the card has been removed
				 */
				RFSetReaderEventState(rContext, SCARD_REMOVED);

				rContext->readerState->cardAtrLength = 0;
				rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;
				rContext->readerState->readerState |= SCARD_ABSENT;
				rContext->readerState->readerState &= ~SCARD_UNKNOWN;
				rContext->readerState->readerState &= ~SCARD_PRESENT;
				rContext->readerState->readerState &= ~SCARD_POWERED;
				rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
				rContext->readerState->readerState &= ~SCARD_SWALLOWED;
				rContext->readerState->readerState &= ~SCARD_SPECIFIC;
				dwCurrentState = SCARD_ABSENT;

				incrementEventCounter(rContext->readerState);

				SYS_MMapSynchronize((void *) rContext->readerState, pageSize);
			}

		}
		else if (dwStatus & SCARD_PRESENT)
		{
			if (dwCurrentState == SCARD_ABSENT ||
				dwCurrentState == SCARD_UNKNOWN)
			{
				/*
				 * Power and reset the card
				 */
				dwAtrLen = MAX_ATR_SIZE;
				rv = IFDPowerICC(rContext, IFD_POWER_UP,
					rContext->readerState->cardAtr,
					&dwAtrLen);
				rContext->readerState->cardAtrLength = dwAtrLen;

				/* the protocol is unset after a power on */
				rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

				if (rv == IFD_SUCCESS)
				{
					rContext->readerState->readerState |= SCARD_PRESENT;
					rContext->readerState->readerState &= ~SCARD_ABSENT;
					rContext->readerState->readerState |= SCARD_POWERED;
					rContext->readerState->readerState |= SCARD_NEGOTIABLE;
					rContext->readerState->readerState &= ~SCARD_SPECIFIC;
					rContext->readerState->readerState &= ~SCARD_UNKNOWN;
					rContext->readerState->readerState &= ~SCARD_SWALLOWED;
				}
				else
				{
					rContext->readerState->readerState |= SCARD_PRESENT;
					rContext->readerState->readerState &= ~SCARD_ABSENT;
					rContext->readerState->readerState |= SCARD_SWALLOWED;
					rContext->readerState->readerState &= ~SCARD_POWERED;
					rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
					rContext->readerState->readerState &= ~SCARD_SPECIFIC;
					rContext->readerState->readerState &= ~SCARD_UNKNOWN;
					rContext->readerState->cardAtrLength = 0;
				}

				dwCurrentState = SCARD_PRESENT;

				incrementEventCounter(rContext->readerState);

				SYS_MMapSynchronize((void *) rContext->readerState, pageSize);

				Log2(PCSC_LOG_INFO, "Card inserted into %s", lpcReader);

				if (rv == IFD_SUCCESS)
				{
					if (rContext->readerState->cardAtrLength > 0)
					{
						LogXxd(PCSC_LOG_INFO, "Card ATR: ",
							rContext->readerState->cardAtr,
							rContext->readerState->cardAtrLength);
					}
					else
						Log1(PCSC_LOG_INFO, "Card ATR: (NULL)");
				}
				else
					Log1(PCSC_LOG_ERROR,"Error powering up card.");
			}
		}

		/*
		 * Sharing may change w/o an event pass it on
		 */

		if (dwReaderSharing != rContext->dwContexts)
		{
			dwReaderSharing = rContext->dwContexts;
			rContext->readerState->readerSharing = dwReaderSharing;
			SYS_MMapSynchronize((void *) rContext->readerState, pageSize);
		}

		if (rContext->pthCardEvent)
		{
			int ret;
			
			ret = rContext->pthCardEvent(rContext->dwSlot);
			if (IFD_NO_SUCH_DEVICE == ret)
				SYS_USleep(PCSCLITE_STATUS_POLL_RATE);
		}
		else
			SYS_USleep(PCSCLITE_STATUS_POLL_RATE);

		if (rContext->dwLockId == 0xFFFF)
		{
			/*
			 * Exit and notify the caller
			 */
			Log1(PCSC_LOG_CRITICAL, "Die");
			rContext->dwLockId = 0;
			SYS_ThreadExit(NULL);
		}
	}
}

