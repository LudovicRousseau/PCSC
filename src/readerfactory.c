/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	Title  : readerfactory.c
	Package: pcsc lite
	Author : David Corcoran
	Date   : 7/27/99
	License: Copyright (C) 1999 David Corcoran
			<corcoran@linuxnet.com>
	Purpose: This keeps track of a list of currently 
	available reader structures.

$Id$

********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <fcntl.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "ifdhandler.h"
#include "ifdwrapper.h"
/*
 * FIX :: This should go 
 */
#include "debuglog.h"

static PREADER_CONTEXT sContexts[PCSCLITE_MAX_CONTEXTS];
static DWORD *dwNumContexts = 0;

LONG RFAllocateReaderSpace(DWORD dwAllocNum)
{

	int i;   					/* Counter */
	LONG rv;       					/* Return tester */

	/*
	 * Zero out everything 
	 */
	i = 0;

	/*
	 * Allocate global dwNumContexts 
	 */
	dwNumContexts = (DWORD *) malloc(sizeof(DWORD));
	*dwNumContexts = 0;

	/*
	 * Allocate each reader structure 
	 */
	for (i = 0; i < dwAllocNum; i++)
	{
		sContexts[i] = (PREADER_CONTEXT) malloc(sizeof(READER_CONTEXT));
	}

	/*
	 * Create public event structures 
	 */
	rv = EHInitializeEventStructures();

	return rv;
}

LONG RFAddReader(LPSTR lpcReader, DWORD dwPort, LPSTR lpcLibrary)
{

	DWORD dwContext, dwContextB, dwGetSize;
	UCHAR ucGetData[1];
	char lpcStripReader[MAX_READERNAME];
	LONG rv, parentNode;

	int i, j, tmplen, psize;

	/*
	 * Zero out everything 
	 */
	dwContext = 0;
	dwContextB = 0;
	dwGetSize = 0;
	rv = 0;
	i = 0;
	j = 0;
	psize = 0;
	ucGetData[0] = 0;
	tmplen = 0;

	if (lpcReader == 0 || lpcLibrary == 0)
	{
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Same name, same port - duplicate reader cannot be used 
	 */
	if (dwNumContexts != 0)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if ((sContexts[i])->vHandle != 0)
			{
				strncpy(lpcStripReader, (sContexts[i])->lpcReader,
					MAX_READERNAME);
				tmplen = strlen(lpcStripReader);
				lpcStripReader[tmplen - 4] = 0;
				if ((strcmp(lpcReader, lpcStripReader) == 0) &&
					(dwPort == (sContexts[i])->dwPort))
				{
					DebugLogA("RFAddReader: Duplicate reader found.");
					return SCARD_E_DUPLICATE_READER;
				}
			}
		}
	}

	/*
	 * We must find an empty spot to put the reader structure 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle == 0)
		{
			dwContext = i;
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{
		/*
		 * No more spots left return 
		 */
		return SCARD_E_NO_MEMORY;
	}

	/*
	 * Check and set the readername to see if it must be enumerated 
	 */
	parentNode = RFSetReaderName(sContexts[dwContext], lpcReader,
		lpcLibrary, dwPort, 0);

	strcpy((sContexts[dwContext])->lpcLibrary, lpcLibrary);
	(sContexts[dwContext])->dwVersion = 0;
	(sContexts[dwContext])->dwPort = dwPort;
	(sContexts[dwContext])->mMutex = 0;
	(sContexts[dwContext])->dwStatus = 0;
	(sContexts[dwContext])->dwBlockStatus = 0;
	(sContexts[dwContext])->dwContexts = 0;
	(sContexts[dwContext])->dwLockId = 0;
	(sContexts[dwContext])->vHandle = 0;
	(sContexts[dwContext])->dwPublicID = 0;
	(sContexts[dwContext])->dwFeeds = 0;
	(sContexts[dwContext])->dwIdentity =
		(dwContext + 1) << (sizeof(DWORD) / 2) * 8;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		(sContexts[dwContext])->psHandles[i].hCard = 0;
	}

	/*
	 * If a clone to this reader exists take some values from that clone 
	 */
	if (parentNode != -1 && parentNode < PCSCLITE_MAX_CONTEXTS
		&& parentNode >= 0)
	{
		(sContexts[dwContext])->dwFeeds = 
		  (sContexts[parentNode])->dwFeeds;
		*(sContexts[dwContext])->dwFeeds += 1;
		(sContexts[dwContext])->vHandle = 
		  (sContexts[parentNode])->vHandle;
		(sContexts[dwContext])->mMutex = 
		  (sContexts[parentNode])->mMutex;
	}

	if ((sContexts[dwContext])->dwFeeds == 0)
	{
		(sContexts[dwContext])->dwFeeds = 
		  (DWORD *)malloc(sizeof(DWORD));

		/* Initialize dwFeeds to 1, otherwise multiple 
		   cloned readers will cause pcscd to crash when 
		   RFUnloadReader unloads the driver library
		   and there are still devices attached using it --mikeg*/

		*(sContexts[dwContext])->dwFeeds = 1;
	}

	if ((sContexts[dwContext])->mMutex == 0)
	{
		(sContexts[dwContext])->mMutex =
			(PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
		SYS_MutexInit((sContexts[dwContext])->mMutex);
	}

	*dwNumContexts += 1;

	rv = RFInitializeReader(sContexts[dwContext]);
	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Cannot connect to reader exit gracefully 
		 */
		/*
		 * Clean up so it is not using needed space 
		 */
		DebugLogB("RFAddReader: %s init failed.", lpcReader);

		(sContexts[dwContext])->dwVersion = 0;
		(sContexts[dwContext])->dwPort = 0;
		(sContexts[dwContext])->mMutex = 0;
		(sContexts[dwContext])->vHandle = 0;
		(sContexts[dwContext])->dwPublicID = 0;
		(sContexts[dwContext])->dwIdentity = 0;

		if (parentNode >= 0)
		{
			*(sContexts[dwContext])->dwFeeds -= 1;
		}

		*dwNumContexts -= 1;

		return rv;
	}

	EHSpawnEventHandler(sContexts[dwContext]);

	/*
	 * Call on the driver to see if there are multiple slots 
	 */

	rv = IFDGetCapabilities((sContexts[dwContext]),
		TAG_IFD_SLOTS_NUMBER, &dwGetSize, ucGetData);

	if (rv != IFD_SUCCESS || dwGetSize != 1 || ucGetData[0] == 0)
	{
		/*
		 * Reader does not have this defined.  Must be a single slot
		 * reader so we can just return SCARD_S_SUCCESS. 
		 */

		return SCARD_S_SUCCESS;

	} else if (rv == IFD_SUCCESS && dwGetSize == 1 && ucGetData[0] == 1)
	{
		/*
		 * Reader has this defined and it only has one slot 
		 */

		return SCARD_S_SUCCESS;

	} else
	{
		/*
		 * Check the number of slots and create a different 
		 * structure for each one accordingly 
		 */

		/*
		 * Initialize the rest of the slots 
		 */

		for (j = 1; j < ucGetData[0]; j++)
		{

			/*
			 * We must find an empty spot to put the 
			 * reader structure 
			 */
			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if ((sContexts[i])->vHandle == 0)
				{
					dwContextB = i;
					break;
				}
			}

			if (i == PCSCLITE_MAX_CONTEXTS)
			{
				/*
				 * No more spots left return 
				 */
				(sContexts[dwContext])->mMutex = 0;
				(sContexts[dwContext])->vHandle = 0;
				(sContexts[dwContext])->dwPublicID = 0;
				(sContexts[dwContext])->dwIdentity = 0;

				return SCARD_E_NO_MEMORY;
			}

			/*
			 * Check and set the readername to see if it must be
			 * enumerated 
			 */
			rv = RFSetReaderName(sContexts[dwContextB], lpcReader,
				lpcLibrary, dwPort, j);

			strcpy((sContexts[dwContextB])->lpcLibrary, lpcLibrary);
			(sContexts[dwContextB])->dwVersion =
			  (sContexts[dwContext])->dwVersion;
			(sContexts[dwContextB])->dwPort =
			  (sContexts[dwContext])->dwPort;
			(sContexts[dwContextB])->mMutex =
			  (sContexts[dwContext])->mMutex;
			(sContexts[dwContextB])->vHandle =
			  (sContexts[dwContext])->vHandle;

			/* Added by Dave - slots did not have a dwFeeds
			   parameter so it was by luck they were working
			*/

			(sContexts[dwContextB])->dwFeeds =
			  (sContexts[dwContext])->dwFeeds;

			(sContexts[dwContextB])->dwStatus = 0;
			(sContexts[dwContextB])->dwBlockStatus = 0;
			(sContexts[dwContextB])->dwContexts = 0;
			(sContexts[dwContextB])->dwLockId = 0;
			(sContexts[dwContextB])->dwPublicID = 0;
			(sContexts[dwContextB])->dwIdentity =
				(dwContextB + 1) << (sizeof(DWORD) / 2) * 8;

			RFBindFunctions(sContexts[dwContextB]);

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				(sContexts[dwContextB])->psHandles[i].hCard = 0;
			}

			/* Added by Dave for multiple slots */
			*(sContexts[dwContextB])->dwFeeds += 1;
			*dwNumContexts += 1;

			EHSpawnEventHandler(sContexts[dwContextB]);
		}

	}

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReader(LPSTR lpcReader, DWORD dwPort)
{

	LONG rv;
	PREADER_CONTEXT sContext;
	int i;

	/*
	 * Zero out everything 
	 */
	i = 0;

	if (lpcReader == 0)
	{
		return SCARD_E_INVALID_VALUE;
	}

	rv = RFReaderInfoNamePort(dwPort, lpcReader, &sContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Try to destroy the thread 
	 */
	rv = EHDestroyEventHandler(sContext);

	rv = RFUnInitializeReader(sContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Destroy and free the mutex 
	 */
	if (*sContext->dwFeeds == 1)
	{
		SYS_MutexDestroy(sContext->mMutex);
		free(sContext->mMutex);
	}

	*sContext->dwFeeds -= 1;

	/* Added by Dave to free the dwFeeds variable */
	if (*sContext->dwFeeds == 0) {
	  free(sContext->dwFeeds);
	  sContext->dwFeeds = 0;
	}

	sContext->dwVersion = 0;
	sContext->dwPort = 0;
	sContext->mMutex = 0;
	sContext->dwStatus = 0;
	sContext->dwBlockStatus = 0;
	sContext->dwContexts = 0;
	sContext->dwSlot = 0;
	sContext->dwLockId = 0;
	sContext->vHandle = 0;
	sContext->dwIdentity = 0;
	sContext->dwPublicID = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		sContext->psHandles[i].hCard = 0;
	}

	*dwNumContexts -= 1;

	while ((rv = RFReaderInfoNamePort(dwPort, lpcReader, &sContext))
		== SCARD_S_SUCCESS)
	{

		EHDestroyEventHandler(sContext);

		rv = RFUnInitializeReader(sContext);
		if (rv != SCARD_S_SUCCESS)
		  {
		    return rv;
		  }
		
		/*
		 * Destroy and free the mutex 
		 */
		if (*sContext->dwFeeds == 1)
		  {
		    SYS_MutexDestroy(sContext->mMutex);
		    free(sContext->mMutex);
		  }

		*sContext->dwFeeds -= 1;

		/* Added by Dave to free the dwFeeds variable */

		if (*sContext->dwFeeds == 0) {
		  free(sContext->dwFeeds);
		  sContext->dwFeeds = 0;
		}

		sContext->dwVersion = 0;
		sContext->dwPort = 0;
		sContext->mMutex = 0;
		sContext->dwStatus = 0;
		sContext->dwBlockStatus = 0;
		sContext->dwContexts = 0;
		sContext->dwSlot = 0;
		sContext->dwLockId = 0;
		sContext->vHandle = 0;
		sContext->dwIdentity = 0;
		sContext->dwPublicID = 0;

		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			sContext->psHandles[i].hCard = 0;
		}

		*dwNumContexts -= 1;

	}

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderName(PREADER_CONTEXT rContext, LPSTR readerName,
	LPSTR libraryName, DWORD dwPort, DWORD dwSlot)
{

	LONG rv=-1;					/* rv is the reader number of the parent
								 * of the clone */
	LONG ret;
	DWORD valueLength;
	UCHAR tagValue;
	static int lastDigit = 0;
	int currentDigit;
	int supportedChannels;
	int usedDigits[PCSCLITE_MAX_CONTEXTS];
	int i;

	currentDigit = -1;
	i = 0;
	rv = -1;
	supportedChannels = 0;
	valueLength = sizeof(tagValue);
	tagValue = 0;

	/*
	 * Clear the taken list 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		usedDigits[i] = 0;
	}

	currentDigit = -1;

	if (dwSlot == 0)
	{
		if (*dwNumContexts != 0)
		{
			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if ((sContexts[i])->vHandle != 0)
				{
					if (strcmp((sContexts[i])->lpcLibrary,
							libraryName) == 0)
					{

						/*
						 * Ask the driver if it supports multiple channels 
						 */
						ret = IFDGetCapabilities((sContexts[i]),
							TAG_IFD_SIMULTANEOUS_ACCESS,
							&valueLength, &tagValue);

						if ((ret == IFD_SUCCESS) && (valueLength == 1) &&
							(tagValue > 1))
						{
							supportedChannels = tagValue;
						} else
						{
							supportedChannels = -1;
						}

						/*
						 * Check to see if it is a hotplug reader and
						 * different 
						 */
						if (((((sContexts[i])->dwPort & 0xFFFF0000) ==
									0x200000)
								&& (sContexts[i])->dwPort != dwPort)
							|| (supportedChannels > 1))
						{

							/*
							 * rv tells the caller who the parent of this
							 * clone is so it can use it's shared
							 * resources like mutex/etc. 
							 */

							rv = i;

							/*
							 * If the same reader already exists and it is 
							 * hotplug then we must look for others and
							 * enumerate the readername 
							 */

							currentDigit = (sContexts[i])->
								lpcReader[strlen((sContexts[i])->
									lpcReader) - 3] - '0';

							if (currentDigit > 9)
							{
								/*
								 * 0-9 -> A-F is 7 apart 
								 */
								currentDigit -= 7;
							}

							/*
							 * This spot is taken 
							 */
							usedDigits[currentDigit] = 1;
						}
					}
				}
			}
		}

		/*
		 * No other identical reader exists on the same bus 
		 */
		if (currentDigit == -1)
		{
			sprintf(rContext->lpcReader, "%s 0 %ld", readerName, dwSlot);
			/*
			 * Set the slot in 0xDDDDCCCC 
			 */
			rContext->dwSlot = dwSlot;
		} else
		{

			for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
			{
				if (usedDigits[i] == 0)
				{
					break;
				}
			}

			if (i == PCSCLITE_MAX_CONTEXTS)
			{
				return -1;
			} else if (i > supportedChannels)
			{
				return -1;
			}

			if (i <= 9)
			{
				sprintf(rContext->lpcReader, "%s %d %ld", readerName, i,
					dwSlot);
			} else
			{
				sprintf(rContext->lpcReader, "%s %c %ld", readerName,
					'A' + (i % 10), dwSlot);
			}

			/*
			 * Set the slot in 0xDDDDCCCC 
			 */
			rContext->dwSlot = (0x00010000 * i) + dwSlot;
			lastDigit = i;
		}

		/*
		 * On the second, third slot of the reader use the last used
		 * reader number 
		 */

	} else
	{
		if (lastDigit <= 9 && dwSlot <= 9)
		{
			sprintf(rContext->lpcReader, "%s %d %ld", readerName,
				lastDigit, dwSlot);
		} else if (lastDigit > 9 && dwSlot <= 9)
		{
			sprintf(rContext->lpcReader, "%s %c %ld", readerName,
				'A' + (lastDigit % 10), dwSlot);
		} else if (lastDigit <= 9 && dwSlot > 9)
		{
			sprintf(rContext->lpcReader, "%s %d %c", readerName,
				lastDigit, 'A' + ((int) dwSlot % 10));
		} else if (lastDigit > 9 && dwSlot > 9)
		{
			sprintf(rContext->lpcReader, "%s %c %c", readerName,
				'A' + (lastDigit % 10), 'A' + ((int) dwSlot % 10));
		}

		rContext->dwSlot = (0x00010000 * lastDigit) + dwSlot;
	}

	return rv;
}

LONG RFListReaders(LPSTR lpcReaders, LPDWORD pdwReaderNum)
{

	DWORD dwCSize;
	LPSTR lpcTReaders;
	int i, p;

	/*
	 * Zero out everything 
	 */
	dwCSize = 0;
	lpcTReaders = 0;
	i = 0;
	p = 0;

	if (*dwNumContexts == 0)
	{
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * Ignore the groups for now, return all readers 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
			dwCSize += strlen((sContexts[i])->lpcReader) + 1;
			p += 1;
		}
	}

	if (p > *dwNumContexts)
	{
		/*
		 * We are severely hosed here 
		 */
		/*
		 * Hopefully this will never be true 
		 */
		return SCARD_F_UNKNOWN_ERROR;
	}

	/*
	 * Added for extra NULL byte on MultiString 
	 */
	dwCSize += 1;

	/*
	 * If lpcReaders is not allocated then just 
	 */
	/*
	 * return the amount needed to allocate 
	 */
	if (lpcReaders == 0)
	{
		*pdwReaderNum = dwCSize;
		return SCARD_S_SUCCESS;
	}

	if (*pdwReaderNum < dwCSize)
	{
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	*pdwReaderNum = dwCSize;
	lpcTReaders = lpcReaders;
	p = 0;

	/*
	 * Creating MultiString 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
			strcpy(&lpcTReaders[p], (sContexts[i])->lpcReader);
			p += strlen((sContexts[i])->lpcReader);	/* Copy */
			lpcTReaders[p] = 0;	/* Add NULL */
			p += 1;	/* Move on */
		}
	}

	lpcTReaders[p] = 0;	/* Add NULL */

	return SCARD_S_SUCCESS;
}

LONG RFReaderInfo(LPSTR lpcReader, PREADER_CONTEXT * sReader)
{

	int i;

	/*
	 * Zero out everything 
	 */
	i = 0;

	if (lpcReader == 0)
	{
		return SCARD_E_UNKNOWN_READER;
	}

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
			if (strcmp(lpcReader, (sContexts[i])->lpcReader) == 0)
			{
				*sReader = sContexts[i];
				return SCARD_S_SUCCESS;
			}
		}
	}

	return SCARD_E_UNKNOWN_READER;
}

LONG RFReaderInfoNamePort(DWORD dwPort, LPSTR lpcReader,
	PREADER_CONTEXT * sReader)
{

	char lpcStripReader[MAX_READERNAME];
	int i, tmplen;

	/*
	 * Zero out everything 
	 */
	i = 0;
	tmplen = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
			strncpy(lpcStripReader, (sContexts[i])->lpcReader,
				MAX_READERNAME);
			tmplen = strlen(lpcStripReader);
			lpcStripReader[tmplen - 4] = 0;

			if ((strcmp(lpcReader, lpcStripReader) == 0) &&
				(dwPort == (sContexts[i])->dwPort))
			{
				*sReader = sContexts[i];
				return SCARD_S_SUCCESS;
			}
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG RFReaderInfoById(DWORD dwIdentity, PREADER_CONTEXT * sReader)
{

	int i;

	/*
	 * Zero out everything 
	 */
	i = 0;

	/*
	 * Strip off the lower nibble and get the identity 
	 */

	dwIdentity = dwIdentity >> (sizeof(DWORD) / 2) * 8;
	dwIdentity = dwIdentity << (sizeof(DWORD) / 2) * 8;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (dwIdentity == (sContexts[i])->dwIdentity)
		{
			*sReader = sContexts[i];
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG RFLoadReader(PREADER_CONTEXT rContext)
{

	LONG rv;

	rv = 0;

	if (rContext->vHandle != 0)
	{
		DebugLogA("RFLoadReader: Warning library pointer not NULL");
		/*
		 * Another reader exists with this library loaded 
		 */
		return SCARD_S_SUCCESS;
	}

	if (rContext->vHandle == 0)
	{
		rv = DYN_LoadLibrary(&rContext->vHandle, rContext->lpcLibrary);
	}

	return rv;
}

LONG RFBindFunctions(PREADER_CONTEXT rContext)
{

	LONG rv, ret;
	LPVOID pvTestA, pvTestB;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	ret = 0;
	pvTestA = 0;
	pvTestB = 0;

	/*
	 * Use this function as a dummy to determine the IFD Handler version
	 * type.  1.0/2.0.  Suppress error messaging since it can't be 1.0 and 
	 * 2.0. 
	 */

	DebugLogSuppress(DEBUGLOG_IGNORE_ENTRIES);

	rv = DYN_GetAddress(rContext->vHandle,
		&rContext->psFunctions.pvfCreateChannel, "IO_Create_Channel");

	ret = DYN_GetAddress(rContext->vHandle,
		&rContext->psFunctions.pvfCreateChannel, "IFDHCreateChannel");

	DebugLogSuppress(DEBUGLOG_LOG_ENTRIES);

	if (rv != SCARD_S_SUCCESS && ret != SCARD_S_SUCCESS)
	{
		/*
		 * Neither version of the IFD Handler was found - exit 
		 */
		rContext->psFunctions.pvfCreateChannel = 0;

		DebugLogA("RFBindFunctions: IFDHandler functions missing");

		exit(1);
	} else if (rv == SCARD_S_SUCCESS)
	{
		/*
		 * Ifd Handler 1.0 found 
		 */
		/*
		 * Re bind the function since it was lost in the second 
		 */
		rContext->dwVersion |= IFD_HVERSION_1_0;
		DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfCreateChannel, "IO_Create_Channel");
	} else
	{
		/*
		 * Ifd Handler 2.0 found 
		 */
		rContext->dwVersion |= IFD_HVERSION_2_0;
	}

	/*
	 * The following binds version 1.0 of the IFD Handler specs 
	 */

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{

		DebugLogA("RFBindFunctions: Loading IFD Handler 1.0");

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfCloseChannel, "IO_Close_Channel");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfCloseChannel = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfGetCapabilities,
			"IFD_Get_Capabilities");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfGetCapabilities = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfSetCapabilities,
			"IFD_Set_Capabilities");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfSetCapabilities = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfSetProtocol,
			"IFD_Set_Protocol_Parameters");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfSetProtocol = 0;
			/*
			 * Not a completely required function 
			 */
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfPowerICC, "IFD_Power_ICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfPowerICC = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfSwallowICC, "IFD_Swallow_ICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfSwallowICC = 0;
			/*
			 * Not a completely required function 
			 */
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfEjectICC, "IFD_Eject_ICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfEjectICC = 0;
			/*
			 * Not a completely required function 
			 */
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfConfiscateICC, "IFD_Confiscate_ICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfConfiscateICC = 0;
			/*
			 * Not a completely required function 
			 */
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfTransmitICC, "IFD_Transmit_to_ICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfTransmitICC = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfICCPresent, "IFD_Is_ICC_Present");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfICCPresent = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfICCAbsent, "IFD_Is_ICC_Absent");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfICCAbsent = 0;
			/*
			 * Not a completely required function 
			 */
		}

		/*
		 * The following binds version 2.0 of the IFD Handler specs 
		 */

	} else if (rContext->dwVersion & IFD_HVERSION_2_0)
	{

		DebugLogA("RFBindFunctions: Loading IFD Handler 2.0");

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfCloseChannel, "IFDHCloseChannel");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfCloseChannel = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfGetCapabilities,
			"IFDHGetCapabilities");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfGetCapabilities = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfSetCapabilities,
			"IFDHSetCapabilities");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfSetCapabilities = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfSetProtocol,
			"IFDHSetProtocolParameters");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfSetProtocol = 0;
			/*
			 * Not a completely required function 
			 */
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfPowerICC, "IFDHPowerICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfPowerICC = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfTransmitICC, "IFDHTransmitToICC");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfTransmitICC = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfControl, "IFDHControl");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfControl = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

		rv = DYN_GetAddress(rContext->vHandle,
			&rContext->psFunctions.pvfICCPresent, "IFDHICCPresence");

		if (rv != SCARD_S_SUCCESS)
		{
			rContext->psFunctions.pvfICCPresent = 0;
			DebugLogA("RFBindFunctions: IFDHandler functions missing");
			exit(1);
		}

	} else
	{
		/*
		 * Who knows what could have happenned for it to get here. 
		 */
		DebugLogA("RFBindFunctions: IFD Handler not 1.0/2.0");
		exit(1);
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnBindFunctions(PREADER_CONTEXT rContext)
{

	/*
	 * Zero out everything 
	 */

	rContext->psFunctions.pvfCreateChannel = 0;
	rContext->psFunctions.pvfCloseChannel = 0;
	rContext->psFunctions.pvfGetCapabilities = 0;
	rContext->psFunctions.pvfSetCapabilities = 0;
	rContext->psFunctions.pvfSetProtocol = 0;
	rContext->psFunctions.pvfPowerICC = 0;
	rContext->psFunctions.pvfSwallowICC = 0;
	rContext->psFunctions.pvfEjectICC = 0;
	rContext->psFunctions.pvfConfiscateICC = 0;
	rContext->psFunctions.pvfTransmitICC = 0;
	rContext->psFunctions.pvfICCPresent = 0;
	rContext->psFunctions.pvfICCAbsent = 0;

	return SCARD_S_SUCCESS;
}

LONG RFUnloadReader(PREADER_CONTEXT rContext)
{

	/*
	 * Make sure no one else is using this library 
	 */

	if (*rContext->dwFeeds == 1)
	{
	  DebugLogA("RFUnloadReader: Unloading reader driver.");
		DYN_CloseLibrary(&rContext->vHandle);
	}

	rContext->vHandle = 0;

	return SCARD_S_SUCCESS;
}

LONG RFCheckSharing(DWORD hCard)
{

	LONG rv;
	DWORD dwSharing;
	PREADER_CONTEXT rContext;

	/*
	 * Zero out everything 
	 */
	dwSharing = 0;
	rContext = 0;

	rv = RFReaderInfoById(hCard, &rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	dwSharing = hCard;

	if (rContext->dwLockId == 0 || rContext->dwLockId == dwSharing)
	{
		return SCARD_S_SUCCESS;
	} else
	{
		return SCARD_E_SHARING_VIOLATION;
	}

}

LONG RFLockSharing(DWORD hCard)
{

	PREADER_CONTEXT rContext;

	/*
	 * Zero out everything 
	 */
	rContext = 0;

	RFReaderInfoById(hCard, &rContext);

	if (RFCheckSharing(hCard) == SCARD_S_SUCCESS)
	{
		EHSetSharingEvent(rContext, 1);
		rContext->dwLockId = hCard;
	} else
	{
		return SCARD_E_SHARING_VIOLATION;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnlockSharing(DWORD hCard)
{

	PREADER_CONTEXT rContext;

	/*
	 * Zero out everything 
	 */
	rContext = 0;

	RFReaderInfoById(hCard, &rContext);

	if (RFCheckSharing(hCard) == SCARD_S_SUCCESS)
	{
		EHSetSharingEvent(rContext, 0);
		rContext->dwLockId = 0;
	} else
	{
		return SCARD_E_SHARING_VIOLATION;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnblockContext(SCARDCONTEXT hContext)
{

	int i;

	/*
	 * Zero out everything 
	 */
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		(sContexts[i])->dwBlockStatus = hContext;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnblockReader(PREADER_CONTEXT rContext)
{

	rContext->dwBlockStatus = BLOCK_STATUS_RESUME;
	return SCARD_S_SUCCESS;
}

LONG RFInitializeReader(PREADER_CONTEXT rContext)
{

	LONG rv;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Spawn the event handler thread 
	 */
	DebugLogB("RFInitializeReader: Attempting startup of %s.",
		rContext->lpcReader);

  /******************************************/
	/*
	 * This section loads the library 
	 */
  /******************************************/
	rv = RFLoadReader(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

  /*******************************************/
	/*
	 * This section binds the functions 
	 */
  /*******************************************/
	rv = RFBindFunctions(rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		RFUnloadReader(rContext);
		return rv;
	}

  /*******************************************/
	/*
	 * This section tries to open the port 
	 */
  /*******************************************/

	rv = IFDOpenIFD(rContext, rContext->dwPort);

	if (rv != IFD_SUCCESS)
	{
		DebugLogB("RFInitializeReader: Open Port %X Failed",
			rContext->dwPort);
		RFUnBindFunctions(rContext);
		RFUnloadReader(rContext);
		return SCARD_E_INVALID_TARGET;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnInitializeReader(PREADER_CONTEXT rContext)
{

	DebugLogB("RFUninitializeReader: Attempting shutdown of %s.",
		rContext->lpcReader);

	/*
	 * Close the port, unbind the functions, and unload the library 
	 */

	/*
	 * If the reader is getting uninitialized then it is being unplugged
	 * so I can't send a IFDPowerICC call to it
	 * 
	 * IFDPowerICC( rContext, IFD_POWER_DOWN, Atr, &AtrLen ); 
	 */
	IFDCloseIFD(rContext);
	RFUnBindFunctions(rContext);
	RFUnloadReader(rContext);

	return SCARD_S_SUCCESS;
}

SCARDHANDLE RFCreateReaderHandle(PREADER_CONTEXT rContext)
{

	int i, j;
	USHORT randHandle;

	/*
	 * Zero out everything 
	 */
	randHandle = 0;
	i = 0;
	j = 0;

	/*
	 * Create a random handle with 16 bits check to see if it already is
	 * used. 
	 */

	randHandle = SYS_Random(SYS_GetSeed(), 10, 65000);

	while (1)
	{
		for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
		{
			if ((sContexts[i])->vHandle != 0)
			{
				for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
				{
					if ((rContext->dwIdentity + randHandle) ==
						(sContexts[i])->psHandles[j].hCard)
					{
						/*
						 * Get a new handle and loop again 
						 */
						randHandle = SYS_Random(randHandle, 10, 65000);
						continue;
					}
				}
			}
		}

		/*
		 * Once the for loop is completed w/o restart a good handle was
		 * found and the loop can be exited. 
		 */

		if (i == PCSCLITE_MAX_CONTEXTS)
		{
			break;
		}
	}

	return rContext->dwIdentity + randHandle;
}

LONG RFFindReaderHandle(SCARDHANDLE hCard)
{

	int i, j;

	i = 0;
	j = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
			for (j = 0; j < PCSCLITE_MAX_CONTEXTS; j++)
			{
				if (hCard == (sContexts[i])->psHandles[j].hCard)
				{
					return SCARD_S_SUCCESS;
				}
			}
		}
	}

	return SCARD_E_INVALID_HANDLE;
}

LONG RFDestroyReaderHandle(SCARDHANDLE hCard)
{

	return SCARD_S_SUCCESS;
}

LONG RFAddReaderHandle(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (rContext->psHandles[i].hCard == 0)
		{
			rContext->psHandles[i].hCard = hCard;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{	/* List is full */
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReaderHandle(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			rContext->psHandles[i].hCard = 0;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{	/* Not Found */
		return SCARD_E_INVALID_HANDLE;
	}

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderEventState(PREADER_CONTEXT rContext, DWORD dwEvent)
{

	int i;
	i = 0;

	/*
	 * Set all the handles for that reader to the event 
	 */
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (rContext->psHandles[i].hCard != 0)
		{
			rContext->psHandles[i].dwEventStatus = dwEvent;
		}
	}

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			if (rContext->psHandles[i].dwEventStatus == SCARD_REMOVED)
			{
				return SCARD_W_REMOVED_CARD;
			} else if (rContext->psHandles[i].dwEventStatus == SCARD_RESET)
			{
				return SCARD_W_RESET_CARD;
			} else if (rContext->psHandles[i].dwEventStatus == 0)
			{
				return SCARD_S_SUCCESS;
			} else
			{
				return SCARD_E_INVALID_VALUE;
			}
		}
	}

	return SCARD_E_INVALID_HANDLE;
}

LONG RFClearReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{

	int i;
	i = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			rContext->psHandles[i].dwEventStatus = 0;
		}
	}

	if (i == PCSCLITE_MAX_CONTEXTS)
	{	/* Not Found */
		return SCARD_E_INVALID_HANDLE;
	}

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderStatus(PREADER_CONTEXT rContext)
{

	if (rContext->dwStatus & SCARD_UNKNOWN)
	{
		return SCARD_E_READER_UNAVAILABLE;
	} else
	{
		return SCARD_S_SUCCESS;
	}
}

void RFCleanupReaders(int shouldExit)
{
	int i;
	LONG rv;
	char lpcStripReader[MAX_READERNAME];

	DebugLogA("RFCleanupReaders: entering cleaning function");
	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if (sContexts[i]->vHandle != 0)
		{
			DebugLogB("Stopping reader: %s", sContexts[i]->lpcReader);

			strncpy(lpcStripReader, (sContexts[i])->lpcReader,
				MAX_READERNAME);
			/*
			 * strip the 4 last char ' 0 0' 
			 */
			lpcStripReader[strlen(lpcStripReader) - 4] = '\0';

			rv = RFRemoveReader(lpcStripReader, sContexts[i]->dwPort);

			if (rv != SCARD_S_SUCCESS)
				DebugLogB("RFRemoveReader error: %s",
					pcsc_stringify_error(rv));
		}
	}

	/*
	 * exit() will call at_exit() 
	 */

        if (shouldExit) 
        {
                exit(0);
        }
}

void RFSuspendAllReaders() 
{
        int i;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
                        EHDestroyEventHandler(sContexts[i]);
                        IFDCloseIFD(sContexts[i]);
		}
	}

}

void RFAwakeAllReaders() 
{
        LONG rv = IFD_SUCCESS;
        int i, j;
        int initFlag;
        
        initFlag = 0;

	for (i = 0; i < PCSCLITE_MAX_CONTEXTS; i++)
	{
		if ((sContexts[i])->vHandle != 0)
		{
                
                        for (j=0; j < i; j++)
                        {
                                if (((sContexts[j])->vHandle == (sContexts[i])->vHandle)&&
                                    ((sContexts[j])->dwPort   == (sContexts[i])->dwPort)) 
                                    {
                                            initFlag = 1;
                                    }
                        }
                        
                        if (initFlag == 0)
                        {
                                rv = IFDOpenIFD(sContexts[i], (sContexts[i])->dwPort);

                        } else {
                                initFlag = 0;
                        }
                        

                                if (rv != IFD_SUCCESS)
                                {
                                        DebugLogB("RFInitializeReader: Open Port %X Failed",
                                                    (sContexts[i])->dwPort);
                                }


                        EHSpawnEventHandler(sContexts[i]);
                        RFSetReaderEventState(sContexts[i], SCARD_RESET);
                        
		}
	}

}
