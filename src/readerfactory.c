/*
 * This keeps track of a list of currently available reader structures.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#include "pcsclite.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "ifdwrapper.h"
#include "hotplug.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static PREADER_CONTEXT sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
static DWORD dwNumReadersContexts = 0;

LONG RFAllocateReaderSpace(void)
{
	int i;   					/* Counter */

	/*
	 * Allocate each reader structure 
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		sReadersContexts[i] = (PREADER_CONTEXT) malloc(sizeof(READER_CONTEXT));
		(sReadersContexts[i])->vHandle = NULL;
		(sReadersContexts[i])->readerState = NULL;
	}

	/*
	 * Create public event structures 
	 */
	return EHInitializeEventStructures();
}

LONG RFAddReader(LPTSTR lpcReader, DWORD dwPort, LPTSTR lpcLibrary, LPTSTR lpcDevice)
{
	DWORD dwContext = 0, dwGetSize;
	UCHAR ucGetData[1], ucThread[1];
	LONG rv, parentNode;
	int i, j;

	if (lpcReader == 0 || lpcLibrary == 0)
		return SCARD_E_INVALID_VALUE;

	/*
	 * Same name, same port - duplicate reader cannot be used 
	 */
	if (dwNumReadersContexts != 0)
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i])->vHandle != 0)
			{
				char lpcStripReader[MAX_READERNAME];
				int tmplen;

				/* get the reader name without the reader and slot numbers */
				strncpy(lpcStripReader, (sReadersContexts[i])->lpcReader,
					sizeof(lpcStripReader));
				tmplen = strlen(lpcStripReader);
				lpcStripReader[tmplen - 6] = 0;

				if ((strcmp(lpcReader, lpcStripReader) == 0) &&
					(dwPort == (sReadersContexts[i])->dwPort))
				{
					DebugLogA("Duplicate reader found.");
					return SCARD_E_DUPLICATE_READER;
				}
			}
		}
	}

	/*
	 * We must find an empty slot to put the reader structure 
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle == 0)
		{
			dwContext = i;
			break;
		}
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		/*
		 * No more spots left return 
		 */
		return SCARD_E_NO_MEMORY;
	}

	/*
	 * Check and set the readername to see if it must be enumerated 
	 */
	parentNode = RFSetReaderName(sReadersContexts[dwContext], lpcReader,
		lpcLibrary, dwPort, 0);

	strcpy((sReadersContexts[dwContext])->lpcLibrary, lpcLibrary);
	strcpy((sReadersContexts[dwContext])->lpcDevice, lpcDevice);
	(sReadersContexts[dwContext])->dwVersion = 0;
	(sReadersContexts[dwContext])->dwPort = dwPort;
	(sReadersContexts[dwContext])->mMutex = 0;
	(sReadersContexts[dwContext])->dwBlockStatus = 0;
	(sReadersContexts[dwContext])->dwContexts = 0;
	(sReadersContexts[dwContext])->pthThread = 0;
	(sReadersContexts[dwContext])->dwLockId = 0;
	(sReadersContexts[dwContext])->vHandle = 0;
	(sReadersContexts[dwContext])->pdwFeeds = 0;
	(sReadersContexts[dwContext])->dwIdentity =
		(dwContext + 1) << (sizeof(DWORD) / 2) * 8;
	(sReadersContexts[dwContext])->readerState = NULL;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
		(sReadersContexts[dwContext])->psHandles[i].hCard = 0;

	/*
	 * If a clone to this reader exists take some values from that clone 
	 */
	if (parentNode >= 0 && parentNode < PCSCLITE_MAX_READERS_CONTEXTS)
	{
		(sReadersContexts[dwContext])->pdwFeeds = 
		  (sReadersContexts[parentNode])->pdwFeeds;
		*(sReadersContexts[dwContext])->pdwFeeds += 1;
		(sReadersContexts[dwContext])->vHandle = 
		  (sReadersContexts[parentNode])->vHandle;
		(sReadersContexts[dwContext])->mMutex = 
		  (sReadersContexts[parentNode])->mMutex;
		(sReadersContexts[dwContext])->pdwMutex = 
		  (sReadersContexts[parentNode])->pdwMutex;

		/*
		 * Call on the driver to see if it is thread safe 
		 */
		dwGetSize = sizeof(ucThread);
		rv = IFDGetCapabilities((sReadersContexts[parentNode]),
		       TAG_IFD_THREAD_SAFE, &dwGetSize, ucThread);

		if (rv == IFD_SUCCESS && dwGetSize == 1 && ucThread[0] == 1)
		{
			DebugLogA("Driver is thread safe");
			(sReadersContexts[dwContext])->mMutex = 0;
			(sReadersContexts[dwContext])->pdwMutex = 0;
		}
		else
			*(sReadersContexts[dwContext])->pdwMutex += 1;
	}

	if ((sReadersContexts[dwContext])->pdwFeeds == 0)
	{
		(sReadersContexts[dwContext])->pdwFeeds = 
		  (DWORD *)malloc(sizeof(DWORD));

		/* Initialize pdwFeeds to 1, otherwise multiple 
		   cloned readers will cause pcscd to crash when 
		   RFUnloadReader unloads the driver library
		   and there are still devices attached using it --mikeg*/

		*(sReadersContexts[dwContext])->pdwFeeds = 1;
	}

	if ((sReadersContexts[dwContext])->mMutex == 0)
	{
		(sReadersContexts[dwContext])->mMutex =
		  (PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
		SYS_MutexInit((sReadersContexts[dwContext])->mMutex);
	}

	if ((sReadersContexts[dwContext])->pdwMutex == 0)
	{
		(sReadersContexts[dwContext])->pdwMutex = 
		  (DWORD *)malloc(sizeof(DWORD));

		*(sReadersContexts[dwContext])->pdwMutex = 1;
	}

	dwNumReadersContexts += 1;

	rv = RFInitializeReader(sReadersContexts[dwContext]);
	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Cannot connect to reader exit gracefully 
		 */
		/*
		 * Clean up so it is not using needed space 
		 */
		DebugLogB("%s init failed.", lpcReader);

		(sReadersContexts[dwContext])->dwVersion = 0;
		(sReadersContexts[dwContext])->dwPort = 0;
		(sReadersContexts[dwContext])->vHandle = 0;
		(sReadersContexts[dwContext])->readerState = NULL;
		(sReadersContexts[dwContext])->dwIdentity = 0;

		/*
		 * Destroy and free the mutex 
		 */
		if (*(sReadersContexts[dwContext])->pdwMutex == 1)
		{
			SYS_MutexDestroy((sReadersContexts[dwContext])->mMutex);
			free((sReadersContexts[dwContext])->mMutex);
		}

		*(sReadersContexts[dwContext])->pdwMutex -= 1;

		if (*(sReadersContexts[dwContext])->pdwMutex == 0)
		{
			free((sReadersContexts[dwContext])->pdwMutex);
			(sReadersContexts[dwContext])->pdwMutex = 0;
		}

		*(sReadersContexts[dwContext])->pdwFeeds -= 1;

		if (*(sReadersContexts[dwContext])->pdwFeeds == 0)
		{
			free((sReadersContexts[dwContext])->pdwFeeds);
			(sReadersContexts[dwContext])->pdwFeeds = 0;
		}

		dwNumReadersContexts -= 1;

		return rv;
	}

	rv = EHSpawnEventHandler(sReadersContexts[dwContext]);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Call on the driver to see if there are multiple slots 
	 */

	dwGetSize = sizeof(ucGetData);
	rv = IFDGetCapabilities((sReadersContexts[dwContext]),
		TAG_IFD_SLOTS_NUMBER, &dwGetSize, ucGetData);

	if (rv != IFD_SUCCESS || dwGetSize != 1 || ucGetData[0] == 0)
		/*
		 * Reader does not have this defined.  Must be a single slot
		 * reader so we can just return SCARD_S_SUCCESS. 
		 */
		return SCARD_S_SUCCESS;

	if (rv == IFD_SUCCESS && dwGetSize == 1 && ucGetData[0] == 1)
		/*
		 * Reader has this defined and it only has one slot 
		 */
		return SCARD_S_SUCCESS;

	/*
	 * Check the number of slots and create a different 
	 * structure for each one accordingly 
	 */

	/*
	 * Initialize the rest of the slots 
	 */

	for (j = 1; j < ucGetData[0]; j++)
	{
		char *tmpReader = NULL;
		DWORD dwContextB = 0;

		/*
		 * We must find an empty spot to put the 
		 * reader structure 
		 */
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i])->vHandle == 0)
			{
				dwContextB = i;
				break;
			}
		}

		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		{
			/*
			 * No more spots left return 
			 */
			rv = RFRemoveReader(lpcReader, dwPort);
			return SCARD_E_NO_MEMORY;
		}

		/*
		 * Copy the previous reader name and increment the slot number
		 */
		tmpReader = sReadersContexts[dwContextB]->lpcReader;
		strcpy(tmpReader, sReadersContexts[dwContext]->lpcReader);
		sprintf(tmpReader + strlen(tmpReader) - 2, "%02X", j);

		strcpy((sReadersContexts[dwContextB])->lpcLibrary, lpcLibrary);
		strcpy((sReadersContexts[dwContextB])->lpcDevice, lpcDevice);
		(sReadersContexts[dwContextB])->dwVersion =
		  (sReadersContexts[dwContext])->dwVersion;
		(sReadersContexts[dwContextB])->dwPort =
		  (sReadersContexts[dwContext])->dwPort;
		(sReadersContexts[dwContextB])->vHandle =
		  (sReadersContexts[dwContext])->vHandle;
		(sReadersContexts[dwContextB])->mMutex =
		   (sReadersContexts[dwContext])->mMutex;
		(sReadersContexts[dwContextB])->pdwMutex =
		   (sReadersContexts[dwContext])->pdwMutex;
		sReadersContexts[dwContextB]->dwSlot =
			sReadersContexts[dwContext]->dwSlot + j;

		/* 
		 * Added by Dave - slots did not have a pdwFeeds
		 * parameter so it was by luck they were working
		 */

		(sReadersContexts[dwContextB])->pdwFeeds =
		  (sReadersContexts[dwContext])->pdwFeeds;

		/* Added by Dave for multiple slots */
		*(sReadersContexts[dwContextB])->pdwFeeds += 1;

		(sReadersContexts[dwContextB])->dwBlockStatus = 0;
		(sReadersContexts[dwContextB])->dwContexts = 0;
		(sReadersContexts[dwContextB])->dwLockId = 0;
		(sReadersContexts[dwContextB])->readerState = NULL;
		(sReadersContexts[dwContextB])->dwIdentity =
			(dwContextB + 1) << (sizeof(DWORD) / 2) * 8;

		for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
			(sReadersContexts[dwContextB])->psHandles[i].hCard = 0;

		/*
		 * Call on the driver to see if the slots are thread safe 
		 */

		dwGetSize = sizeof(ucThread);
		rv = IFDGetCapabilities((sReadersContexts[dwContext]),
			TAG_IFD_SLOT_THREAD_SAFE, &dwGetSize, ucThread);

		if (rv == IFD_SUCCESS && dwGetSize == 1 && ucThread[0] == 1)
		{
			(sReadersContexts[dwContextB])->mMutex =
				(PCSCLITE_MUTEX_T) malloc(sizeof(PCSCLITE_MUTEX));
			SYS_MutexInit((sReadersContexts[dwContextB])->mMutex);

			(sReadersContexts[dwContextB])->pdwMutex = 
				(DWORD *)malloc(sizeof(DWORD));
			*(sReadersContexts[dwContextB])->pdwMutex = 1;
		}
		else
			*(sReadersContexts[dwContextB])->pdwMutex += 1;

		dwNumReadersContexts += 1;

		rv = RFInitializeReader(sReadersContexts[dwContextB]);
		if (rv != SCARD_S_SUCCESS)
		{
			/*
			 * Cannot connect to slot exit gracefully 
			 */
			/*
			 * Clean up so it is not using needed space 
			 */
			DebugLogB("%s init failed.", lpcReader);

			(sReadersContexts[dwContextB])->dwVersion = 0;
			(sReadersContexts[dwContextB])->dwPort = 0;
			(sReadersContexts[dwContextB])->vHandle = 0;
			(sReadersContexts[dwContextB])->readerState = NULL;
			(sReadersContexts[dwContextB])->dwIdentity = 0;

			/*
			 * Destroy and free the mutex 
			 */
			if (*(sReadersContexts[dwContextB])->pdwMutex == 1)
			{
				SYS_MutexDestroy((sReadersContexts[dwContextB])->mMutex);
				free((sReadersContexts[dwContextB])->mMutex);
			}

			*(sReadersContexts[dwContextB])->pdwMutex -= 1;

			if (*(sReadersContexts[dwContextB])->pdwMutex == 0)
			{
				free((sReadersContexts[dwContextB])->pdwMutex);
				(sReadersContexts[dwContextB])->pdwMutex = 0;
			}

			*(sReadersContexts[dwContextB])->pdwFeeds -= 1;

			if (*(sReadersContexts[dwContextB])->pdwFeeds == 0)
			{
				free((sReadersContexts[dwContextB])->pdwFeeds);
				(sReadersContexts[dwContextB])->pdwFeeds = 0;
			}

			dwNumReadersContexts -= 1;

			return rv;
		}

		EHSpawnEventHandler(sReadersContexts[dwContextB]);
	}

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReader(LPTSTR lpcReader, DWORD dwPort)
{
	LONG rv;
	PREADER_CONTEXT sContext;

	if (lpcReader == 0)
		return SCARD_E_INVALID_VALUE;

	while ((rv = RFReaderInfoNamePort(dwPort, lpcReader, &sContext))
		== SCARD_S_SUCCESS)
	{
		int i;

		/*
		 * Try to destroy the thread 
		 */
		rv = EHDestroyEventHandler(sContext);

		rv = RFUnInitializeReader(sContext);
		if (rv != SCARD_S_SUCCESS)
			return rv;

		/*
		 * Destroy and free the mutex 
		 */
		if ((NULL == sContext->pdwMutex) || (NULL == sContext->pdwFeeds))
		{
			DebugLogA("Trying to remove an already removed driver");
			return SCARD_E_INVALID_VALUE;
		}

		if (*sContext->pdwMutex == 1)
		{
			SYS_MutexDestroy(sContext->mMutex);
			free(sContext->mMutex);
		}

		*sContext->pdwMutex -= 1;

		if (*sContext->pdwMutex == 0)
		{
			free(sContext->pdwMutex);
			sContext->pdwMutex = NULL;
		}

		*sContext->pdwFeeds -= 1;

		/* Added by Dave to free the pdwFeeds variable */

		if (*sContext->pdwFeeds == 0)
		{
			free(sContext->pdwFeeds);
			sContext->pdwFeeds = NULL;
		}

		sContext->lpcDevice[0] = 0;
		sContext->dwVersion = 0;
		sContext->dwPort = 0;
		sContext->mMutex = 0;
		sContext->dwBlockStatus = 0;
		sContext->dwContexts = 0;
		sContext->dwSlot = 0;
		sContext->dwLockId = 0;
		sContext->vHandle = 0;
		sContext->dwIdentity = 0;
		sContext->readerState = NULL;

		for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
			sContext->psHandles[i].hCard = 0;

		dwNumReadersContexts -= 1;
	}

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderName(PREADER_CONTEXT rContext, LPTSTR readerName,
	LPTSTR libraryName, DWORD dwPort, DWORD dwSlot)
{
	LONG parent = -1;	/* reader number of the parent of the clone */
	DWORD valueLength;
	int currentDigit = -1;
	int supportedChannels = 0;
	int usedDigits[PCSCLITE_MAX_READERS_CONTEXTS];
	int i;

	/*
	 * Clear the list 
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		usedDigits[i] = FALSE;

	if ((0 == dwSlot) && (dwNumReadersContexts != 0))
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i])->vHandle != 0)
			{
				if (strcmp((sReadersContexts[i])->lpcLibrary, libraryName) == 0)
				{
					UCHAR tagValue[1];
					LONG ret;

					/*
					 * Ask the driver if it supports multiple channels 
					 */
					valueLength = sizeof(tagValue);
					ret = IFDGetCapabilities((sReadersContexts[i]),
						TAG_IFD_SIMULTANEOUS_ACCESS,
						&valueLength, tagValue);

					if ((ret == IFD_SUCCESS) && (valueLength == 1) &&
						(tagValue[0] > 1))
					{
						supportedChannels = tagValue[0];
						DebugLogB("Support %d simultaneous readers",
							tagValue[0]);
					}
					else
						supportedChannels = -1;

					/*
					 * Check to see if it is a hotplug reader and
					 * different 
					 */
					if (((((sReadersContexts[i])->dwPort & 0xFFFF0000) ==
							PCSCLITE_HP_BASE_PORT)
						&& ((sReadersContexts[i])->dwPort != dwPort))
						|| (supportedChannels > 1))
					{
						char *lpcReader = sReadersContexts[i]->lpcReader;

						/*
						 * tells the caller who the parent of this
						 * clone is so it can use it's shared
						 * resources like mutex/etc. 
						 */
						parent = i;

						/*
						 * If the same reader already exists and it is 
						 * hotplug then we must look for others and
						 * enumerate the readername 
						 */
						currentDigit = strtol(lpcReader + strlen(lpcReader) - 5, NULL, 16);

						/*
						 * This spot is taken 
						 */
						usedDigits[currentDigit] = TRUE;
					}
				}
			}
		}

	}

	/* default value */
	i = 0;

	/* Other identical readers exist on the same bus */
	if (currentDigit != -1)
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			/* get the first free digit */
			if (usedDigits[i] == FALSE)
				break;
		}

		if ((i == PCSCLITE_MAX_READERS_CONTEXTS) || (i > supportedChannels))
			return -1;
	}

	sprintf(rContext->lpcReader, "%s %02X %02lX", readerName, i, dwSlot);

	/*
	 * Set the slot in 0xDDDDCCCC 
	 */
	rContext->dwSlot = (i << 16) + dwSlot;

	return parent;
}

LONG RFListReaders(LPTSTR lpcReaders, LPDWORD pdwReaderNum)
{
	DWORD dwCSize;
	LPTSTR lpcTReaders;
	int i, p;

	if (dwNumReadersContexts == 0)
		return SCARD_E_READER_UNAVAILABLE;

	/*
	 * Ignore the groups for now, return all readers 
	 */
	dwCSize = 0;
	p = 0;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			dwCSize += strlen((sReadersContexts[i])->lpcReader) + 1;
			p += 1;
		}
	}

	if (p > dwNumReadersContexts)
		/*
		 * We are severely hosed here 
		 */
		/*
		 * Hopefully this will never be true 
		 */
		return SCARD_F_UNKNOWN_ERROR;

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
		return SCARD_E_INSUFFICIENT_BUFFER;

	*pdwReaderNum = dwCSize;
	lpcTReaders = lpcReaders;
	p = 0;

	/*
	 * Creating MultiString 
	 */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			strcpy(&lpcTReaders[p], (sReadersContexts[i])->lpcReader);
			p += strlen((sReadersContexts[i])->lpcReader);	/* Copy */
			lpcTReaders[p] = 0;	/* Add NULL */
			p += 1;	/* Move on */
		}
	}

	lpcTReaders[p] = 0;	/* Add NULL */

	return SCARD_S_SUCCESS;
}

LONG RFReaderInfo(LPTSTR lpcReader, PREADER_CONTEXT * sReader)
{
	int i;

	if (lpcReader == 0)
		return SCARD_E_UNKNOWN_READER;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			if (strcmp(lpcReader, (sReadersContexts[i])->lpcReader) == 0)
			{
				*sReader = sReadersContexts[i];
				return SCARD_S_SUCCESS;
			}
		}
	}

	return SCARD_E_UNKNOWN_READER;
}

LONG RFReaderInfoNamePort(DWORD dwPort, LPTSTR lpcReader,
	PREADER_CONTEXT * sReader)
{
	char lpcStripReader[MAX_READERNAME];
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			int tmplen;

			strncpy(lpcStripReader, (sReadersContexts[i])->lpcReader,
				sizeof(lpcStripReader));
			tmplen = strlen(lpcStripReader);
			lpcStripReader[tmplen - 6] = 0;

			if ((strcmp(lpcReader, lpcStripReader) == 0) &&
				(dwPort == (sReadersContexts[i])->dwPort))
			{
				*sReader = sReadersContexts[i];
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
	 * Strip off the lower nibble and get the identity 
	 */
	dwIdentity = dwIdentity >> (sizeof(DWORD) / 2) * 8;
	dwIdentity = dwIdentity << (sizeof(DWORD) / 2) * 8;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (dwIdentity == (sReadersContexts[i])->dwIdentity)
		{
			*sReader = sReadersContexts[i];
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG RFLoadReader(PREADER_CONTEXT rContext)
{
	if (rContext->vHandle != 0)
	{
		DebugLogA("Warning library pointer not NULL");
		/*
		 * Another reader exists with this library loaded 
		 */
		return SCARD_S_SUCCESS;
	}

	return DYN_LoadLibrary(&rContext->vHandle, rContext->lpcLibrary);
}

LONG RFBindFunctions(PREADER_CONTEXT rContext)
{
	int rv1, rv2, rv3;

	/*
	 * Use this function as a dummy to determine the IFD Handler version
	 * type  1.0/2.0/3.0.  Suppress error messaging since it can't be 1.0,
	 * 2.0 and 3.0. 
	 */

	DebugLogSuppress(DEBUGLOG_IGNORE_ENTRIES);

	rv1 = DYN_GetAddress(rContext->vHandle,
		(void **)&rContext->psFunctions.psFunctions_v1.pvfCreateChannel,
		"IO_Create_Channel");

	rv2 = DYN_GetAddress(rContext->vHandle,
		(void **)&rContext->psFunctions.psFunctions_v2.pvfCreateChannel,
		"IFDHCreateChannel");

	rv3 = DYN_GetAddress(rContext->vHandle,
		(void **)&rContext->psFunctions.psFunctions_v3.pvfCreateChannelByName,
		"IFDHCreateChannelByName");

	DebugLogSuppress(DEBUGLOG_LOG_ENTRIES);

	if (rv1 != SCARD_S_SUCCESS && rv2 != SCARD_S_SUCCESS && rv3 != SCARD_S_SUCCESS)
	{
		/*
		 * Neither version of the IFD Handler was found - exit 
		 */
		DebugLogA("IFDHandler functions missing");

		exit(1);
	} else if (rv1 == SCARD_S_SUCCESS)
	{
		/*
		 * Ifd Handler 1.0 found 
		 */
		rContext->dwVersion = IFD_HVERSION_1_0;
	} else if (rv3 == SCARD_S_SUCCESS)
	{
		/*
		 * Ifd Handler 3.0 found 
		 */
		rContext->dwVersion = IFD_HVERSION_3_0;
	}
	else
	{
		/*
		 * Ifd Handler 2.0 found 
		 */
		rContext->dwVersion = IFD_HVERSION_2_0;
	}

	/*
	 * The following binds version 1.0 of the IFD Handler specs 
	 */

	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		DebugLogA("Loading IFD Handler 1.0");

#define GET_ADDRESS_OPTIONALv1(field, function, code) \
{ \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, (void **)&rContext->psFunctions.psFunctions_v1.pvf ## field, "IFD_" #function)) \
	{ \
		rContext->psFunctions.psFunctions_v1.pvf ## field = NULL; \
		code \
	} \
}

#define GET_ADDRESSv1(field, function) \
	GET_ADDRESS_OPTIONALv1(field, function, \
		DebugLogA("IFDHandler functions missing: " #function ); \
		exit(1); )

		DYN_GetAddress(rContext->vHandle,
			(void **)&rContext->psFunctions.psFunctions_v1.pvfCreateChannel,
			"IO_Create_Channel");

		if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle,
			(void **)&rContext->psFunctions.psFunctions_v1.pvfCloseChannel,
			"IO_Close_Channel"))
		{
			rContext->psFunctions.psFunctions_v1.pvfCloseChannel = NULL;
			DebugLogA("IFDHandler functions missing");
			exit(1);
		}

		GET_ADDRESSv1(GetCapabilities, Get_Capabilities)
		GET_ADDRESSv1(SetCapabilities, Set_Capabilities)
		GET_ADDRESSv1(PowerICC, Power_ICC)
		GET_ADDRESSv1(TransmitToICC, Transmit_to_ICC)
		GET_ADDRESSv1(ICCPresence, Is_ICC_Present)

		GET_ADDRESS_OPTIONALv1(SetProtocolParameters, Set_Protocol_Parameters, )
	}
	else if (rContext->dwVersion == IFD_HVERSION_2_0)
	{
		/*
		 * The following binds version 2.0 of the IFD Handler specs 
		 */

#define GET_ADDRESS_OPTIONALv2(s, code) \
{ \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, (void **)&rContext->psFunctions.psFunctions_v2.pvf ## s, "IFDH" #s)) \
	{ \
		rContext->psFunctions.psFunctions_v2.pvf ## s = NULL; \
		code \
	} \
}

#define GET_ADDRESSv2(s) \
	GET_ADDRESS_OPTIONALv2(s, \
		DebugLogA("IFDHandler functions missing: " #s ); \
		exit(1); )

		DebugLogA("Loading IFD Handler 2.0");

		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv2(Control)
	}
	else if (rContext->dwVersion == IFD_HVERSION_3_0)
	{
		/*
		 * The following binds version 3.0 of the IFD Handler specs 
		 */

#define GET_ADDRESS_OPTIONALv3(s, code) \
{ \
	if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, (void **)&rContext->psFunctions.psFunctions_v3.pvf ## s, "IFDH" #s)) \
	{ \
		rContext->psFunctions.psFunctions_v3.pvf ## s = NULL; \
		code \
	} \
}

#define GET_ADDRESSv3(s) \
	GET_ADDRESS_OPTIONALv3(s, \
		DebugLogA("IFDHandler functions missing: " #s ); \
		exit(1); )

		DebugLogA("Loading IFD Handler 3.0");

		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv3(Control)
	}
	else
	{
		/*
		 * Who knows what could have happenned for it to get here. 
		 */
		DebugLogA("IFD Handler not 1.0/2.0 or 3.0");
		exit(1);
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnBindFunctions(PREADER_CONTEXT rContext)
{
	/*
	 * Zero out everything 
	 */

	memset(&rContext->psFunctions, 0, sizeof(rContext->psFunctions));

	return SCARD_S_SUCCESS;
}

LONG RFUnloadReader(PREADER_CONTEXT rContext)
{
	/*
	 * Make sure no one else is using this library 
	 */

	if (*rContext->pdwFeeds == 1)
	{
		DebugLogA("Unloading reader driver.");
		DYN_CloseLibrary(&rContext->vHandle);
	}

	rContext->vHandle = 0;

	return SCARD_S_SUCCESS;
}

LONG RFCheckSharing(DWORD hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	rv = RFReaderInfoById(hCard, &rContext);

	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (rContext->dwLockId == 0 || rContext->dwLockId == hCard)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_SHARING_VIOLATION;

}

LONG RFLockSharing(DWORD hCard)
{
	PREADER_CONTEXT rContext = NULL;

	RFReaderInfoById(hCard, &rContext);

	if (RFCheckSharing(hCard) == SCARD_S_SUCCESS)
	{
		EHSetSharingEvent(rContext, 1);
		rContext->dwLockId = hCard;
	}
	else
		return SCARD_E_SHARING_VIOLATION;

	return SCARD_S_SUCCESS;
}

LONG RFUnlockSharing(DWORD hCard)
{
	PREADER_CONTEXT rContext = NULL;

	RFReaderInfoById(hCard, &rContext);

	if (RFCheckSharing(hCard) == SCARD_S_SUCCESS)
	{
		EHSetSharingEvent(rContext, 0);
		rContext->dwLockId = 0;
	}
	else
		return SCARD_E_SHARING_VIOLATION;

	return SCARD_S_SUCCESS;
}

LONG RFUnblockContext(SCARDCONTEXT hContext)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		(sReadersContexts[i])->dwBlockStatus = hContext;

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
	 * Spawn the event handler thread 
	 */
	DebugLogB("Attempting startup of %s.", rContext->lpcReader);

  /******************************************/
	/*
	 * This section loads the library 
	 */
  /******************************************/
	rv = RFLoadReader(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

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

	rv = IFDOpenIFD(rContext);

	if (rv != IFD_SUCCESS)
	{
		DebugLogC("Open Port %X Failed (%s)",
			rContext->dwPort, rContext->lpcDevice);
		RFUnBindFunctions(rContext);
		RFUnloadReader(rContext);
		return SCARD_E_INVALID_TARGET;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnInitializeReader(PREADER_CONTEXT rContext)
{
	DebugLogB("Attempting shutdown of %s.",
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
	USHORT randHandle;

	/*
	 * Create a random handle with 16 bits check to see if it already is
	 * used. 
	 */
	randHandle = SYS_Random(SYS_GetSeed(), 10, 65000);

	while (1)
	{
		int i;

		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i])->vHandle != 0)
			{
				int j;

				for (j = 0; j < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; j++)
				{
					if ((rContext->dwIdentity + randHandle) ==
						(sReadersContexts[i])->psHandles[j].hCard)
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

		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
			break;
	}

	return rContext->dwIdentity + randHandle;
}

LONG RFFindReaderHandle(SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			int j;

			for (j = 0; j < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; j++)
			{
				if (hCard == (sReadersContexts[i])->psHandles[j].hCard)
					return SCARD_S_SUCCESS;
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

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == 0)
		{
			rContext->psHandles[i].hCard = hCard;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* List is full */
		return SCARD_E_INSUFFICIENT_BUFFER;

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReaderHandle(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			rContext->psHandles[i].hCard = 0;
			rContext->psHandles[i].dwEventStatus = 0;
			break;
		}
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* Not Found */
		return SCARD_E_INVALID_HANDLE;

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderEventState(PREADER_CONTEXT rContext, DWORD dwEvent)
{
	int i;

	/*
	 * Set all the handles for that reader to the event 
	 */
	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard != 0)
			rContext->psHandles[i].dwEventStatus = dwEvent;
	}

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
		{
			if (rContext->psHandles[i].dwEventStatus == SCARD_REMOVED)
				return SCARD_W_REMOVED_CARD;
			else
			{
				if (rContext->psHandles[i].dwEventStatus == SCARD_RESET)
					return SCARD_W_RESET_CARD;
				else
				{
					if (rContext->psHandles[i].dwEventStatus == 0)
						return SCARD_S_SUCCESS;
					else
						return SCARD_E_INVALID_VALUE;
				}
			}
		}
	}

	return SCARD_E_INVALID_HANDLE;
}

LONG RFClearReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READER_CONTEXT_CHANNELS; i++)
	{
		if (rContext->psHandles[i].hCard == hCard)
			rContext->psHandles[i].dwEventStatus = 0;
	}

	if (i == PCSCLITE_MAX_READER_CONTEXT_CHANNELS)
		/* Not Found */
		return SCARD_E_INVALID_HANDLE;

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderStatus(PREADER_CONTEXT rContext)
{
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN))
		return SCARD_E_READER_UNAVAILABLE;
	else
		return SCARD_S_SUCCESS;
}

void RFCleanupReaders(int shouldExit)
{
	int i;

	DebugLogA("entering cleaning function");
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (sReadersContexts[i]->vHandle != 0)
		{
			LONG rv;
			char lpcStripReader[MAX_READERNAME];

			DebugLogB("Stopping reader: %s", sReadersContexts[i]->lpcReader);

			strncpy(lpcStripReader, (sReadersContexts[i])->lpcReader,
				sizeof(lpcStripReader));
			/*
			 * strip the 6 last char ' 00 00' 
			 */
			lpcStripReader[strlen(lpcStripReader) - 6] = '\0';

			rv = RFRemoveReader(lpcStripReader, sReadersContexts[i]->dwPort);

			if (rv != SCARD_S_SUCCESS)
				DebugLogB("RFRemoveReader error: %s",
					pcsc_stringify_error(rv));
		}
	}

	/*
	 * exit() will call at_exit() 
	 */

	if (shouldExit) 
		exit(0);
}

void RFSuspendAllReaders(void) 
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			EHDestroyEventHandler(sReadersContexts[i]);
			IFDCloseIFD(sReadersContexts[i]);
		}
	}

}

void RFAwakeAllReaders(void) 
{
	LONG rv = IFD_SUCCESS;
	int i;
	int initFlag;
        
	initFlag = 0;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		/* If the library is loaded and the event handler is not running */
		if ( ((sReadersContexts[i])->vHandle   != 0) &&
		     ((sReadersContexts[i])->pthThread == 0) )
		{
			int j;

			for (j=0; j < i; j++)
			{
				if (((sReadersContexts[j])->vHandle == (sReadersContexts[i])->vHandle)&&
					((sReadersContexts[j])->dwPort   == (sReadersContexts[i])->dwPort)) 
				{
					initFlag = 1;
				}
			}
                        
			if (initFlag == 0)
				rv = IFDOpenIFD(sReadersContexts[i]);
			else
				initFlag = 0;

			if (rv != IFD_SUCCESS)
			{
				DebugLogC("Open Port %X Failed (%s)",
					(sReadersContexts[i])->dwPort, (sReadersContexts[i])->lpcDevice);
			}


			EHSpawnEventHandler(sReadersContexts[i]);
			RFSetReaderEventState(sReadersContexts[i], SCARD_RESET);
		}
	}
}

