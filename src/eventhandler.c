/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : eventhandler.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 3/13/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This keeps track of card insertion/removal events
	    and updates ATR, protocol, and status information.

********************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "eventhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "ifdhandler.h"
#include "ifdwrapper.h"
#include "debuglog.h"
#include "prothandler.h"

static PREADER_STATES readerStates[PCSCLITE_MAX_CONTEXTS];

void EHStatusHandlerThread(PREADER_CONTEXT);

LONG EHInitializeEventStructures()
{

	int fd, i, pageSize;

	fd = 0;
	i = 0;
	pageSize = 0;

	SYS_RemoveFile(PCSCLITE_PUBSHM_FILE);

	fd = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDWR | O_CREAT, 00644);
	if (fd < 0)
	{
		DebugLogA("Error: Cannot open public shared file");
		exit(1);
	}

	SYS_Chmod(PCSCLITE_PUBSHM_FILE,
		S_IRGRP | S_IREAD | S_IWRITE | S_IROTH);

	pageSize = SYS_GetPageSize();

	/*
	 * Jump to end of file space and allocate zero's 
	 */
	SYS_SeekFile(fd, pageSize * PCSCLITE_MAX_CONTEXTS);
	SYS_WriteFile(fd, "", 1);

	/*
	 * Allocate each reader structure 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		readerStates[i] = (PREADER_STATES)
			SYS_MemoryMap(sizeof(READER_STATES), fd, (i * pageSize));
		if (readerStates[i] == 0)
		{
			DebugLogA("Error: Cannot public memory map");
			exit(1);
		}

		/*
		 * Zero out each value in the struct 
		 */
		memset((readerStates[i])->readerName, 0, MAX_READERNAME);
		memset((readerStates[i])->cardAtr, 0, MAX_ATR_SIZE);
		(readerStates[i])->readerID = 0;
		(readerStates[i])->readerState = 0;
		(readerStates[i])->lockState = 0;
		(readerStates[i])->readerSharing = 0;
		(readerStates[i])->cardAtrLength = 0;
		(readerStates[i])->cardProtocol = 0;
	}

	return SCARD_S_SUCCESS;
}

LONG EHDestroyEventHandler(PREADER_CONTEXT rContext)
{

	LONG rv;
	int i;

	i = 0;
	rv = 0;
        
        
	i = rContext->dwPublicID;
        if ((readerStates[i])->readerName[0] == 0)
        {
                DebugLogA("EHDestroyEventHandler: Thread already stomped.");
                return SCARD_S_SUCCESS;
        }

	/*
	 * Set the thread to 0 to exit thread 
	 */
	rContext->dwLockId = 0xFFFF;

	DebugLogA("EHDestroyEventHandler: Stomping thread.");

	do
	{
		/*
		 * Wait 0.05 seconds for the child to respond 
		 */
		SYS_USleep(50000);
	}
	while (rContext->dwLockId == 0xFFFF);

	/*
	 * Zero out the public status struct to allow it to be recycled and
	 * used again 
	 */

	memset((readerStates[i])->readerName, 0, MAX_READERNAME);
	memset((readerStates[i])->cardAtr, 0, MAX_ATR_SIZE);
	(readerStates[i])->readerID = 0;
	(readerStates[i])->readerState = 0;
	(readerStates[i])->lockState = 0;
	(readerStates[i])->readerSharing = 0;
	(readerStates[i])->cardAtrLength = 0;
	(readerStates[i])->cardProtocol = 0;

	DebugLogA("EHDestroyEventHandler: Thread stomped.");

	return SCARD_S_SUCCESS;
}

LONG EHSpawnEventHandler(PREADER_CONTEXT rContext)
{

	LONG rv;
	LPCSTR lpcReader;
	DWORD dwStatus, dwProtocol;
	int i;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	lpcReader = 0;
	dwStatus = 0;
	dwProtocol = 0;
	i = 0;

	lpcReader = rContext->lpcReader;

	rv = IFDStatusICC(rContext, &dwStatus,
		&dwProtocol, rContext->ucAtr, &rContext->dwAtrLen);

	if (rv != SCARD_S_SUCCESS)
	{
		DebugLogB("EHSpawnEventHandler: Initial Check Failed on %s",
			lpcReader);
		return SCARD_F_UNKNOWN_ERROR;
	}

	/*
	 * Find an empty reader slot and insert the new reader 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((readerStates[i])->readerID == 0)
		{
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		return SCARD_F_INTERNAL_ERROR;
	}

	/*
	 * Set all the attributes to this reader 
	 */
	strcpy((readerStates[i])->readerName, rContext->lpcReader);
	memcpy((readerStates[i])->cardAtr, rContext->ucAtr,
		rContext->dwAtrLen);
	(readerStates[i])->readerID = i + 100;
	(readerStates[i])->readerState = rContext->dwStatus;
	(readerStates[i])->readerSharing = rContext->dwContexts;
	(readerStates[i])->cardAtrLength = rContext->dwAtrLen;
	(readerStates[i])->cardProtocol = rContext->dwProtocol;
	/*
	 * So the thread can access this array indice 
	 */
	rContext->dwPublicID = i;

	rv = SYS_ThreadCreate(&rContext->pthThread, NULL,
		(LPVOID) EHStatusHandlerThread, (LPVOID) rContext);
	if (rv == 1)
	{
		return SCARD_S_SUCCESS;
	} else
	{
		return SCARD_E_NO_MEMORY;
	}

}

void EHStatusHandlerThread(PREADER_CONTEXT rContext)
{

	LONG rv;
	LPCSTR lpcReader;
	DWORD dwStatus, dwProtocol, dwReaderSharing;
	DWORD dwErrorCount, dwCurrentState;
	int i, pageSize;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	lpcReader = 0;
	dwStatus = 0;
	dwProtocol = 0;
	dwReaderSharing = 0;
	dwCurrentState = 0;
	dwErrorCount = 0;
	i = 0;
	pageSize = 0;

	lpcReader = rContext->lpcReader;
	i = rContext->dwPublicID;

	pageSize = SYS_GetPageSize();

	rv = IFDStatusICC(rContext, &dwStatus,
		&dwProtocol, rContext->ucAtr, &rContext->dwAtrLen);

	if (dwStatus & SCARD_PRESENT)
	{
		rv = IFDPowerICC(rContext, IFD_POWER_UP,
			rContext->ucAtr, &rContext->dwAtrLen);

		if (rv == IFD_SUCCESS)
		{
			rContext->dwProtocol = PHGetDefaultProtocol(rContext->ucAtr,
				rContext->dwAtrLen);
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_POWERED;
			rContext->dwStatus |= SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
		} else
		{
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_POWERED;
			rContext->dwStatus &= ~SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
			rContext->dwProtocol = 0;
			rContext->dwAtrLen = 0;
		}

		dwCurrentState = SCARD_PRESENT;

	} else
	{
		dwCurrentState = SCARD_ABSENT;
		rContext->dwStatus |= SCARD_ABSENT;
		rContext->dwStatus &= ~SCARD_PRESENT;
		rContext->dwStatus &= ~SCARD_POWERED;
		rContext->dwStatus &= ~SCARD_NEGOTIABLE;
		rContext->dwStatus &= ~SCARD_SPECIFIC;
		rContext->dwStatus &= ~SCARD_SWALLOWED;
		rContext->dwStatus &= ~SCARD_UNKNOWN;
		rContext->dwAtrLen = 0;
		rContext->dwProtocol = 0;
	}

	/*
	 * Set all the public attributes to this reader 
	 */
	(readerStates[i])->readerState = rContext->dwStatus;
	(readerStates[i])->cardAtrLength = rContext->dwAtrLen;
	(readerStates[i])->cardProtocol = rContext->dwProtocol;
	(readerStates[i])->readerSharing = dwReaderSharing =
		rContext->dwContexts;
	memcpy((readerStates[i])->cardAtr, rContext->ucAtr,
		rContext->dwAtrLen);

	SYS_MMapSynchronize((void *) readerStates[i], pageSize);

	while (1)
	{

		dwStatus = 0;

		rv = IFDStatusICC(rContext, &dwStatus,
			&dwProtocol, rContext->ucAtr, &rContext->dwAtrLen);

		if (rv != SCARD_S_SUCCESS)
		{
			DebugLogB("EHSpawnEventHandler: Error communicating to: %s",
				lpcReader);

			/*
			 * Set error status on this reader while errors occur 
			 */

			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus &= ~SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_POWERED;
			rContext->dwStatus &= ~SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_SWALLOWED;
			rContext->dwStatus |= SCARD_UNKNOWN;
			rContext->dwAtrLen = 0;
			rContext->dwProtocol = 0;

			dwCurrentState = SCARD_UNKNOWN;

			/*
			 * Set all the public attributes to this reader 
			 */
			(readerStates[i])->readerState = rContext->dwStatus;
			(readerStates[i])->cardAtrLength = rContext->dwAtrLen;
			(readerStates[i])->cardProtocol = rContext->dwProtocol;
			memcpy((readerStates[i])->cardAtr, rContext->ucAtr,
				rContext->dwAtrLen);

			SYS_MMapSynchronize((void *) readerStates[i], pageSize);

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
				DebugLogB("EHSpawnEventHandler: Card Removed From %s",
					lpcReader);
				/*
				 * Notify the card has been removed 
				 */
				RFSetReaderEventState(rContext, SCARD_REMOVED);

				rContext->dwAtrLen = 0;
				rContext->dwProtocol = 0;
				rContext->dwStatus |= SCARD_ABSENT;
				rContext->dwStatus &= ~SCARD_UNKNOWN;
				rContext->dwStatus &= ~SCARD_PRESENT;
				rContext->dwStatus &= ~SCARD_POWERED;
				rContext->dwStatus &= ~SCARD_NEGOTIABLE;
				rContext->dwStatus &= ~SCARD_SWALLOWED;
				rContext->dwStatus &= ~SCARD_SPECIFIC;
				dwCurrentState = SCARD_ABSENT;

				/*
				 * Set all the public attributes to this reader 
				 */
				(readerStates[i])->readerState = rContext->dwStatus;
				(readerStates[i])->cardAtrLength = rContext->dwAtrLen;
				(readerStates[i])->cardProtocol = rContext->dwProtocol;
				memcpy((readerStates[i])->cardAtr, rContext->ucAtr,
					rContext->dwAtrLen);

				SYS_MMapSynchronize((void *) readerStates[i], pageSize);
			}

		} else if (dwStatus & SCARD_PRESENT)
		{
			if (dwCurrentState == SCARD_ABSENT ||
				dwCurrentState == SCARD_UNKNOWN)
			{

				/*
				 * Power and reset the card 
				 */
				SYS_USleep(PCSCLITE_STATUS_WAIT);
				rv = IFDPowerICC(rContext, IFD_POWER_UP,
					rContext->ucAtr, &rContext->dwAtrLen);

				if (rv == IFD_SUCCESS)
				{
					rContext->dwProtocol =
						PHGetDefaultProtocol(rContext->ucAtr,
						rContext->dwAtrLen);
					rContext->dwStatus |= SCARD_PRESENT;
					rContext->dwStatus &= ~SCARD_ABSENT;
					rContext->dwStatus |= SCARD_POWERED;
					rContext->dwStatus |= SCARD_NEGOTIABLE;
					rContext->dwStatus &= ~SCARD_SPECIFIC;
					rContext->dwStatus &= ~SCARD_UNKNOWN;
					rContext->dwStatus &= ~SCARD_SWALLOWED;

					/*
					 * Notify the card has been reset 
					 */
					/*
					 * RFSetReaderEventState( rContext, SCARD_RESET ); 
					 */
				} else
				{
					rContext->dwStatus |= SCARD_PRESENT;
					rContext->dwStatus &= ~SCARD_ABSENT;
					rContext->dwStatus |= SCARD_SWALLOWED;
					rContext->dwStatus &= ~SCARD_POWERED;
					rContext->dwStatus &= ~SCARD_NEGOTIABLE;
					rContext->dwStatus &= ~SCARD_SPECIFIC;
					rContext->dwStatus &= ~SCARD_UNKNOWN;
					rContext->dwAtrLen = 0;
					rContext->dwProtocol = 0;
				}

				dwCurrentState = SCARD_PRESENT;

				/*
				 * Set all the public attributes to this reader 
				 */
				(readerStates[i])->readerState = rContext->dwStatus;
				(readerStates[i])->cardAtrLength = rContext->dwAtrLen;
				(readerStates[i])->cardProtocol = rContext->dwProtocol;
				memcpy((readerStates[i])->cardAtr, rContext->ucAtr,
					rContext->dwAtrLen);

				SYS_MMapSynchronize((void *) readerStates[i], pageSize);

				DebugLogB("EHSpawnEventHandler: Card inserted into %s",
					lpcReader);

				if (rv == IFD_SUCCESS)
				{
					if (rContext->dwAtrLen > 0)
					{
						DebugXxd("EHSpawnEventHandler: Card ATR: ",
							rContext->ucAtr, rContext->dwAtrLen);
					} else
					{
						DebugLogA("EHSpawnEventHandler: Card ATR: (NULL)");
					}

				} else
				{
					DebugLogA
						("EHSpawnEventHandler: Error powering up card.");
				}
			}
		}

		if (rContext->dwLockId == 0xFFFF)
		{
			/*
			 * Exit and notify the caller 
			 */
			rContext->dwLockId = 0;
			SYS_ThreadDetach(rContext->pthThread);
			SYS_ThreadExit(0);
		}

		/*
		 * Sharing may change w/o an event pass it on 
		 */

		if (dwReaderSharing != rContext->dwContexts)
		{
			dwReaderSharing = rContext->dwContexts;
			(readerStates[i])->readerSharing = dwReaderSharing;
			SYS_MMapSynchronize((void *) readerStates[i], pageSize);
		}

		SYS_USleep(PCSCLITE_STATUS_POLL_RATE);
	}
}

void EHSetSharingEvent(PREADER_CONTEXT rContext, DWORD dwValue)
{

	(readerStates[rContext->dwPublicID])->lockState = dwValue;

}
