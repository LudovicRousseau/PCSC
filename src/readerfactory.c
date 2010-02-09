/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2009
 *  Jean-Luc Giraud <jlgiraud@googlemail.com>
 *
 * $Id$
 */

/**
 * @file
 * @brief This keeps track of a list of currently available reader structures.
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

#include "misc.h"
#include "pcscd.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "thread_generic.h"
#include "readerfactory.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "ifdwrapper.h"
#include "hotplug.h"
#include "strlcpycat.h"
#include "configfile.h"
#include "utils.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static READER_CONTEXT * sReadersContexts[PCSCLITE_MAX_READERS_CONTEXTS];
static int maxReaderHandles = PCSC_MAX_READER_HANDLES;
static DWORD dwNumReadersContexts = 0;
static char *ConfigFile = NULL;
static int ConfigFileCRC = 0;
static PCSCLITE_MUTEX LockMutex = PTHREAD_MUTEX_INITIALIZER;

#define IDENTITY_SHIFT 16

static int RDR_CLIHANDLES_seeker(const void *el, const void *key)
{
	const RDR_CLIHANDLES *rdrCliHandles = el;

	if ((el == NULL) || (key == NULL))
		Log3(PCSC_LOG_CRITICAL,
			"RDR_CLIHANDLES_seeker called with NULL pointer: el=%X, key=%X",
			el, key);

	if (rdrCliHandles->hCard == *(SCARDHANDLE *)key)
		return 1;

	return 0;
}


LONG RFAllocateReaderSpace(unsigned int customMaxReaderHandles)
{
	int i;	/* Counter */

	if (customMaxReaderHandles != 0)
		maxReaderHandles = customMaxReaderHandles;

	/* Allocate each reader structure */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		sReadersContexts[i] = malloc(sizeof(READER_CONTEXT));
		(sReadersContexts[i])->vHandle = NULL;
		(sReadersContexts[i])->readerState = NULL;
	}

	/* Create public event structures */
	return EHInitializeEventStructures();
}

LONG RFAddReader(LPSTR lpcReader, int port, LPSTR lpcLibrary, LPSTR lpcDevice)
{
	DWORD dwContext = 0, dwGetSize;
	UCHAR ucGetData[1], ucThread[1];
	LONG rv, parentNode;
	int i, j;
	int lrv = 0;

	if ((lpcReader == NULL) || (lpcLibrary == NULL) || (lpcDevice == NULL))
		return SCARD_E_INVALID_VALUE;

	/* Reader name too long? also count " 00 00"*/
	if (strlen(lpcReader) > MAX_READERNAME - sizeof(" 00 00"))
	{
		Log3(PCSC_LOG_ERROR, "Reader name too long: %d chars instead of max %d",
			strlen(lpcReader), MAX_READERNAME - sizeof(" 00 00"));
		return SCARD_E_INVALID_VALUE;
	}

	/* Device name too long? */
	if (strlen(lpcDevice) >= MAX_DEVICENAME)
	{
		Log3(PCSC_LOG_ERROR, "Device name too long: %d chars instead of max %d",
			strlen(lpcDevice), MAX_DEVICENAME);
		return SCARD_E_INVALID_VALUE;
	}

	/* Same name, same port - duplicate reader cannot be used */
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
					(port == (sReadersContexts[i])->port))
				{
					Log1(PCSC_LOG_ERROR, "Duplicate reader found.");
					return SCARD_E_DUPLICATE_READER;
				}
			}
		}
	}

	/* We must find an empty slot to put the reader structure */
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
		/* No more spots left return */
		return SCARD_E_NO_MEMORY;
	}

	/* Check and set the readername to see if it must be enumerated */
	parentNode = RFSetReaderName(sReadersContexts[dwContext], lpcReader,
		lpcLibrary, port, 0);
	if (parentNode < -1)
		return SCARD_E_NO_MEMORY;

	sReadersContexts[dwContext]->lpcLibrary = strdup(lpcLibrary);
	(void)strlcpy((sReadersContexts[dwContext])->lpcDevice, lpcDevice,
		sizeof((sReadersContexts[dwContext])->lpcDevice));
	(sReadersContexts[dwContext])->version = 0;
	(sReadersContexts[dwContext])->port = port;
	(sReadersContexts[dwContext])->mMutex = NULL;
	(sReadersContexts[dwContext])->contexts = 0;
	(sReadersContexts[dwContext])->pthThread = 0;
	(sReadersContexts[dwContext])->hLockId = 0;
	(sReadersContexts[dwContext])->LockCount = 0;
	(sReadersContexts[dwContext])->vHandle = NULL;
	(sReadersContexts[dwContext])->pFeeds = NULL;
	(sReadersContexts[dwContext])->pMutex = NULL;
	(sReadersContexts[dwContext])->dwIdentity =
		(dwContext + 1) << IDENTITY_SHIFT;
	(sReadersContexts[dwContext])->readerState = NULL;

	lrv = list_init(&((sReadersContexts[dwContext])->handlesList));
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %X", lrv);
		return SCARD_E_NO_MEMORY;
	}

	lrv = list_attributes_seeker(&((sReadersContexts[dwContext])->handlesList),
		RDR_CLIHANDLES_seeker);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_attributes_seeker failed with return value: %X", lrv);
		return SCARD_E_NO_MEMORY;
	}

	/* If a clone to this reader exists take some values from that clone */
	if (parentNode >= 0 && parentNode < PCSCLITE_MAX_READERS_CONTEXTS)
	{
		(sReadersContexts[dwContext])->pFeeds =
		  (sReadersContexts[parentNode])->pFeeds;
		*(sReadersContexts[dwContext])->pFeeds += 1;
		(sReadersContexts[dwContext])->vHandle =
		  (sReadersContexts[parentNode])->vHandle;
		(sReadersContexts[dwContext])->mMutex =
		  (sReadersContexts[parentNode])->mMutex;
		(sReadersContexts[dwContext])->pMutex =
		  (sReadersContexts[parentNode])->pMutex;

		/* Call on the parent driver to see if it is thread safe */
		dwGetSize = sizeof(ucThread);
		rv = IFDGetCapabilities((sReadersContexts[parentNode]),
			TAG_IFD_THREAD_SAFE, &dwGetSize, ucThread);

		if (rv == IFD_SUCCESS && dwGetSize == 1 && ucThread[0] == 1)
		{
			Log1(PCSC_LOG_INFO, "Driver is thread safe");
			(sReadersContexts[dwContext])->mMutex = NULL;
			(sReadersContexts[dwContext])->pMutex = NULL;
		}
		else
			*(sReadersContexts[dwContext])->pMutex += 1;
	}

	if ((sReadersContexts[dwContext])->pFeeds == NULL)
	{
		(sReadersContexts[dwContext])->pFeeds = malloc(sizeof(int));

		/* Initialize pFeeds to 1, otherwise multiple
		   cloned readers will cause pcscd to crash when
		   RFUnloadReader unloads the driver library
		   and there are still devices attached using it --mikeg*/
		*(sReadersContexts[dwContext])->pFeeds = 1;
	}

	if ((sReadersContexts[dwContext])->mMutex == 0)
	{
		(sReadersContexts[dwContext])->mMutex =
			malloc(sizeof(PCSCLITE_MUTEX));
		(void)SYS_MutexInit((sReadersContexts[dwContext])->mMutex);
	}

	if ((sReadersContexts[dwContext])->pMutex == NULL)
	{
		(sReadersContexts[dwContext])->pMutex = malloc(sizeof(int));
		*(sReadersContexts[dwContext])->pMutex = 1;
	}

	dwNumReadersContexts += 1;

	rv = RFInitializeReader(sReadersContexts[dwContext]);
	if (rv != SCARD_S_SUCCESS)
	{
		/* Cannot connect to reader. Exit gracefully */
		Log2(PCSC_LOG_ERROR, "%s init failed.", lpcReader);
		(void)RFRemoveReader(lpcReader, port);
		return rv;
	}

	/* asynchronous card movement?  */
	{
		RESPONSECODE (*fct)(DWORD) = NULL;

		dwGetSize = sizeof(fct);

		rv = IFDGetCapabilities((sReadersContexts[dwContext]),
			TAG_IFD_POLLING_THREAD, &dwGetSize, (PUCHAR)&fct);
		if ((rv != SCARD_S_SUCCESS) || (dwGetSize != sizeof(fct)))
		{
			fct = NULL;
			Log1(PCSC_LOG_INFO, "Using the pcscd polling thread");
		}
		else
			Log1(PCSC_LOG_INFO, "Using the reader polling thread");

		rv = EHSpawnEventHandler(sReadersContexts[dwContext], fct);
		if (rv != SCARD_S_SUCCESS)
		{
			Log2(PCSC_LOG_ERROR, "%s init failed.", lpcReader);
			(void)RFRemoveReader(lpcReader, port);
			return rv;
		}
	}

	/* Call on the driver to see if there are multiple slots */
	dwGetSize = sizeof(ucGetData);
	rv = IFDGetCapabilities((sReadersContexts[dwContext]),
		TAG_IFD_SLOTS_NUMBER, &dwGetSize, ucGetData);

	if (rv != IFD_SUCCESS || dwGetSize != 1 || ucGetData[0] == 0)
		/* Reader does not have this defined.  Must be a single slot
		 * reader so we can just return SCARD_S_SUCCESS. */
		return SCARD_S_SUCCESS;

	if (rv == IFD_SUCCESS && dwGetSize == 1 && ucGetData[0] == 1)
		/* Reader has this defined and it only has one slot */
		return SCARD_S_SUCCESS;

	/*
	 * Check the number of slots and create a different
	 * structure for each one accordingly
	 */

	/* Initialize the rest of the slots */
	for (j = 1; j < ucGetData[0]; j++)
	{
		char *tmpReader = NULL;
		DWORD dwContextB = 0;
		RESPONSECODE (*fct)(DWORD) = NULL;

		/* We must find an empty spot to put the reader structure */
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
			/* No more slot left return */
			rv = RFRemoveReader(lpcReader, port);
			return SCARD_E_NO_MEMORY;
		}

		/* Copy the previous reader name and increment the slot number */
		tmpReader = sReadersContexts[dwContextB]->lpcReader;
		(void)strlcpy(tmpReader, sReadersContexts[dwContext]->lpcReader,
			sizeof(sReadersContexts[dwContextB]->lpcReader));
		sprintf(tmpReader + strlen(tmpReader) - 2, "%02X", j);

		sReadersContexts[dwContextB]->lpcLibrary =
			sReadersContexts[dwContext]->lpcLibrary;
		(void)strlcpy((sReadersContexts[dwContextB])->lpcDevice, lpcDevice,
			sizeof((sReadersContexts[dwContextB])->lpcDevice));
		(sReadersContexts[dwContextB])->version =
		  (sReadersContexts[dwContext])->version;
		(sReadersContexts[dwContextB])->port =
		  (sReadersContexts[dwContext])->port;
		(sReadersContexts[dwContextB])->vHandle =
		  (sReadersContexts[dwContext])->vHandle;
		(sReadersContexts[dwContextB])->mMutex =
		  (sReadersContexts[dwContext])->mMutex;
		(sReadersContexts[dwContextB])->pMutex =
		  (sReadersContexts[dwContext])->pMutex;
		sReadersContexts[dwContextB]->slot =
			sReadersContexts[dwContext]->slot + j;

		/*
		 * Added by Dave - slots did not have a pFeeds
		 * parameter so it was by luck they were working
		 */
		(sReadersContexts[dwContextB])->pFeeds =
		  (sReadersContexts[dwContext])->pFeeds;

		/* Added by Dave for multiple slots */
		*(sReadersContexts[dwContextB])->pFeeds += 1;

		(sReadersContexts[dwContextB])->contexts = 0;
		(sReadersContexts[dwContextB])->hLockId = 0;
		(sReadersContexts[dwContextB])->LockCount = 0;
		(sReadersContexts[dwContextB])->readerState = NULL;
		(sReadersContexts[dwContextB])->dwIdentity =
			(dwContextB + 1) << IDENTITY_SHIFT;

		lrv = list_init(&((sReadersContexts[dwContextB])->handlesList));
		if (lrv < 0)
		{
			Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %X", lrv);
			return SCARD_E_NO_MEMORY;
		}

		lrv = list_attributes_seeker(&((sReadersContexts[dwContextB])->handlesList),
			RDR_CLIHANDLES_seeker);
		if (lrv < 0)
		{
			Log2(PCSC_LOG_CRITICAL,
					"list_attributes_seeker failed with return value: %X", lrv);
			return SCARD_E_NO_MEMORY;
		}

		/* Call on the parent driver to see if the slots are thread safe */
		dwGetSize = sizeof(ucThread);
		rv = IFDGetCapabilities((sReadersContexts[dwContext]),
			TAG_IFD_SLOT_THREAD_SAFE, &dwGetSize, ucThread);

		if (rv == IFD_SUCCESS && dwGetSize == 1 && ucThread[0] == 1)
		{
			(sReadersContexts[dwContextB])->mMutex =
				malloc(sizeof(PCSCLITE_MUTEX));
			(void)SYS_MutexInit((sReadersContexts[dwContextB])->mMutex);

			(sReadersContexts[dwContextB])->pMutex = malloc(sizeof(int));
			*(sReadersContexts[dwContextB])->pMutex = 1;
		}
		else
			*(sReadersContexts[dwContextB])->pMutex += 1;

		dwNumReadersContexts += 1;

		rv = RFInitializeReader(sReadersContexts[dwContextB]);
		if (rv != SCARD_S_SUCCESS)
		{
			/* Cannot connect to slot. Exit gracefully */
			(void)RFRemoveReader(lpcReader, port);
			return rv;
		}

		/* asynchronous card movement? */
		dwGetSize = sizeof(fct);

		rv = IFDGetCapabilities((sReadersContexts[dwContextB]),
				TAG_IFD_POLLING_THREAD, &dwGetSize, (PUCHAR)&fct);
		if ((rv != SCARD_S_SUCCESS) || (dwGetSize != sizeof(fct)))
		{
			fct = NULL;
			Log1(PCSC_LOG_INFO, "Using the pcscd polling thread");
		}
		else
			Log1(PCSC_LOG_INFO, "Using the reader polling thread");

		rv = EHSpawnEventHandler(sReadersContexts[dwContextB], fct);
		if (rv != SCARD_S_SUCCESS)
		{
			Log2(PCSC_LOG_ERROR, "%s init failed.", lpcReader);
			(void)RFRemoveReader(lpcReader, port);
			return rv;
		}
	}

	return SCARD_S_SUCCESS;
}

LONG RFRemoveReader(LPSTR lpcReader, int port)
{
	LONG rv;
	READER_CONTEXT * sContext;

	if (lpcReader == 0)
		return SCARD_E_INVALID_VALUE;

	while (SCARD_S_SUCCESS ==
		RFReaderInfoNamePort(port, lpcReader, &sContext))
	{

		/* Try to destroy the thread */
		rv = EHDestroyEventHandler(sContext);

		rv = RFUnInitializeReader(sContext);
		if (rv != SCARD_S_SUCCESS)
			return rv;

		/* Destroy and free the mutex */
		if ((NULL == sContext->pMutex) || (NULL == sContext->pFeeds))
		{
			Log1(PCSC_LOG_ERROR,
				"Trying to remove an already removed driver");
			return SCARD_E_INVALID_VALUE;
		}

		/* free shared resources when the last slot is closed */
		if (*sContext->pMutex == 1)
		{
			(void)SYS_MutexDestroy(sContext->mMutex);
			free(sContext->mMutex);
			free(sContext->lpcLibrary);
		}

		*sContext->pMutex -= 1;

		if (*sContext->pMutex == 0)
		{
			free(sContext->pMutex);
			sContext->pMutex = NULL;
		}

		*sContext->pFeeds -= 1;

		/* Added by Dave to free the pFeeds variable */

		if (*sContext->pFeeds == 0)
		{
			free(sContext->pFeeds);
			sContext->pFeeds = NULL;
		}

		sContext->lpcDevice[0] = 0;
		sContext->version = 0;
		sContext->port = 0;
		sContext->mMutex = NULL;
		sContext->contexts = 0;
		sContext->slot = 0;
		sContext->hLockId = 0;
		sContext->LockCount = 0;
		sContext->vHandle = NULL;
		sContext->dwIdentity = 0;
		sContext->readerState = NULL;

		while (list_size(&(sContext->handlesList)) != 0)
		{
			int lrv;
			RDR_CLIHANDLES *currentHandle;

			currentHandle = list_get_at(&(sContext->handlesList), 0);
			lrv = list_delete_at(&(sContext->handlesList), 0);
			if (lrv < 0)
				Log2(PCSC_LOG_CRITICAL,
					"list_delete_at failed with return value: %X", lrv);

			free(currentHandle);
		}
		list_destroy(&(sContext->handlesList));
		dwNumReadersContexts -= 1;

		/* signal an event to clients */
		(void)EHSignalEventToClients();
	}

	return SCARD_S_SUCCESS;
}

LONG RFSetReaderName(READER_CONTEXT * rContext, LPSTR readerName,
	LPSTR libraryName, int port, DWORD slot)
{
	LONG parent = -1;	/* reader number of the parent of the clone */
	DWORD valueLength;
	int currentDigit = -1;
	int supportedChannels = 0;
	int usedDigits[PCSCLITE_MAX_READERS_CONTEXTS];
	int i;

	/* Clear the list */
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		usedDigits[i] = FALSE;

	if ((0 == slot) && (dwNumReadersContexts != 0))
	{
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if ((sReadersContexts[i])->vHandle != 0)
			{
				if (strcmp((sReadersContexts[i])->lpcLibrary, libraryName) == 0)
				{
					UCHAR tagValue[1];
					LONG ret;

					/* Ask the driver if it supports multiple channels */
					valueLength = sizeof(tagValue);
					ret = IFDGetCapabilities((sReadersContexts[i]),
						TAG_IFD_SIMULTANEOUS_ACCESS,
						&valueLength, tagValue);

					if ((ret == IFD_SUCCESS) && (valueLength == 1) &&
						(tagValue[0] > 1))
					{
						supportedChannels = tagValue[0];
						Log2(PCSC_LOG_INFO,
							"Support %d simultaneous readers", tagValue[0]);
					}
					else
						supportedChannels = 1;

					/* Check to see if it is a hotplug reader and different */
					if (((((sReadersContexts[i])->port & 0xFFFF0000) ==
							PCSCLITE_HP_BASE_PORT)
						&& ((sReadersContexts[i])->port != port))
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

						/* This spot is taken */
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

		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		{
			Log2(PCSC_LOG_ERROR, "Max number of readers reached: %d", PCSCLITE_MAX_READERS_CONTEXTS);
			return -2;
		}

		if (i >= supportedChannels)
		{
			Log3(PCSC_LOG_ERROR, "Driver %s does not support more than "
				"%d reader(s). Maybe the driver should support "
				"TAG_IFD_SIMULTANEOUS_ACCESS", libraryName, supportedChannels);
			return -2;
		}
	}

	snprintf(rContext->lpcReader, sizeof(rContext->lpcReader), "%s %02X %02lX",
		readerName, i, slot);

	/* Set the slot in 0xDDDDCCCC */
	rContext->slot = (i << 16) + slot;

	return parent;
}

LONG RFReaderInfo(LPSTR lpcReader, READER_CONTEXT ** sReader)
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

LONG RFReaderInfoNamePort(int port, LPSTR lpcReader,
	READER_CONTEXT * * sReader)
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
				(port == (sReadersContexts[i])->port))
			{
				*sReader = sReadersContexts[i];
				return SCARD_S_SUCCESS;
			}
		}
	}

	return SCARD_E_INVALID_VALUE;
}

LONG RFReaderInfoById(DWORD dwIdentity, READER_CONTEXT * * sReader)
{
	int i;

	/* Strip off the lower nibble and get the identity */
	dwIdentity = dwIdentity >> IDENTITY_SHIFT;
	dwIdentity = dwIdentity << IDENTITY_SHIFT;

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

LONG RFLoadReader(READER_CONTEXT * rContext)
{
	if (rContext->vHandle != 0)
	{
		Log2(PCSC_LOG_INFO, "Reusing already loaded driver for %s",
			rContext->lpcLibrary);
		/* Another reader exists with this library loaded */
		return SCARD_S_SUCCESS;
	}

	return DYN_LoadLibrary(&rContext->vHandle, rContext->lpcLibrary);
}

LONG RFBindFunctions(READER_CONTEXT * rContext)
{
	int rv1, rv2, rv3;
	void *f;

	/*
	 * Use this function as a dummy to determine the IFD Handler version
	 * type  1.0/2.0/3.0.  Suppress error messaging since it can't be 1.0,
	 * 2.0 and 3.0.
	 */

	DebugLogSuppress(DEBUGLOG_IGNORE_ENTRIES);

	rv1 = DYN_GetAddress(rContext->vHandle, &f, "IO_Create_Channel");
	rv2 = DYN_GetAddress(rContext->vHandle, &f, "IFDHCreateChannel");
	rv3 = DYN_GetAddress(rContext->vHandle, &f, "IFDHCreateChannelByName");

	DebugLogSuppress(DEBUGLOG_LOG_ENTRIES);

	if (rv1 != SCARD_S_SUCCESS && rv2 != SCARD_S_SUCCESS && rv3 != SCARD_S_SUCCESS)
	{
		/* Neither version of the IFD Handler was found - exit */
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing");

		return SCARD_F_UNKNOWN_ERROR;
	} else if (rv1 == SCARD_S_SUCCESS)
	{
		/* Ifd Handler 1.0 found */
		rContext->version = IFD_HVERSION_1_0;
	} else if (rv3 == SCARD_S_SUCCESS)
	{
		/* Ifd Handler 3.0 found */
		rContext->version = IFD_HVERSION_3_0;
	}
	else
	{
		/* Ifd Handler 2.0 found */
		rContext->version = IFD_HVERSION_2_0;
	}

	/* The following binds version 1.0 of the IFD Handler specs */
	if (rContext->version == IFD_HVERSION_1_0)
	{
		Log1(PCSC_LOG_INFO, "Loading IFD Handler 1.0");

#define GET_ADDRESS_OPTIONALv1(field, function, code) \
{ \
	void *f1 = NULL; \
	DWORD rv = DYN_GetAddress(rContext->vHandle, &f1, "IFD_" #function); \
	if (SCARD_S_SUCCESS != rv) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v1.pvf ## field = f1; \
}

#define GET_ADDRESSv1(field, function) \
	GET_ADDRESS_OPTIONALv1(field, function, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #function ); \
		return(rv); )

		(void)DYN_GetAddress(rContext->vHandle, &f, "IO_Create_Channel");
		rContext->psFunctions.psFunctions_v1.pvfCreateChannel = f;

		if (SCARD_S_SUCCESS != DYN_GetAddress(rContext->vHandle, &f,
			"IO_Close_Channel"))
		{
			Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing");
			return SCARD_F_UNKNOWN_ERROR;
		}
		rContext->psFunctions.psFunctions_v1.pvfCloseChannel = f;

		GET_ADDRESSv1(GetCapabilities, Get_Capabilities)
		GET_ADDRESSv1(SetCapabilities, Set_Capabilities)
		GET_ADDRESSv1(PowerICC, Power_ICC)
		GET_ADDRESSv1(TransmitToICC, Transmit_to_ICC)
		GET_ADDRESSv1(ICCPresence, Is_ICC_Present)

		GET_ADDRESS_OPTIONALv1(SetProtocolParameters, Set_Protocol_Parameters, )
	}
	else if (rContext->version == IFD_HVERSION_2_0)
	{
		/* The following binds version 2.0 of the IFD Handler specs */
#define GET_ADDRESS_OPTIONALv2(s, code) \
{ \
	void *f1 = NULL; \
	DWORD rv = DYN_GetAddress(rContext->vHandle, &f1, "IFDH" #s); \
	if (SCARD_S_SUCCESS != rv) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v2.pvf ## s = f1; \
}

#define GET_ADDRESSv2(s) \
	GET_ADDRESS_OPTIONALv2(s, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #s ); \
		return(rv); )

		Log1(PCSC_LOG_INFO, "Loading IFD Handler 2.0");

		GET_ADDRESSv2(CreateChannel)
		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv2(Control)
	}
	else if (rContext->version == IFD_HVERSION_3_0)
	{
		/* The following binds version 3.0 of the IFD Handler specs */
#define GET_ADDRESS_OPTIONALv3(s, code) \
{ \
	void *f1 = NULL; \
	DWORD rv = DYN_GetAddress(rContext->vHandle, &f1, "IFDH" #s); \
	if (SCARD_S_SUCCESS != rv) \
	{ \
		code \
	} \
	rContext->psFunctions.psFunctions_v3.pvf ## s = f1; \
}

#define GET_ADDRESSv3(s) \
	GET_ADDRESS_OPTIONALv3(s, \
		Log1(PCSC_LOG_CRITICAL, "IFDHandler functions missing: " #s ); \
		return(rv); )

		Log1(PCSC_LOG_INFO, "Loading IFD Handler 3.0");

		GET_ADDRESSv2(CreateChannel)
		GET_ADDRESSv2(CloseChannel)
		GET_ADDRESSv2(GetCapabilities)
		GET_ADDRESSv2(SetCapabilities)
		GET_ADDRESSv2(PowerICC)
		GET_ADDRESSv2(TransmitToICC)
		GET_ADDRESSv2(ICCPresence)
		GET_ADDRESS_OPTIONALv2(SetProtocolParameters, )

		GET_ADDRESSv3(CreateChannelByName)
		GET_ADDRESSv3(Control)
	}
	else
	{
		/* Who knows what could have happenned for it to get here. */
		Log1(PCSC_LOG_CRITICAL, "IFD Handler not 1.0/2.0 or 3.0");
		return SCARD_F_UNKNOWN_ERROR;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnBindFunctions(READER_CONTEXT * rContext)
{
	/* Zero out everything */
	memset(&rContext->psFunctions, 0, sizeof(rContext->psFunctions));

	return SCARD_S_SUCCESS;
}

LONG RFUnloadReader(READER_CONTEXT * rContext)
{
	/* Make sure no one else is using this library */
	if (*rContext->pFeeds == 1)
	{
		Log1(PCSC_LOG_INFO, "Unloading reader driver.");
		(void)DYN_CloseLibrary(&rContext->vHandle);
	}

	rContext->vHandle = NULL;

	return SCARD_S_SUCCESS;
}

LONG RFCheckSharing(SCARDHANDLE hCard)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	rv = RFReaderInfoById(hCard, &rContext);

	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (rContext->hLockId == 0 || rContext->hLockId == hCard)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_SHARING_VIOLATION;
}

LONG RFLockSharing(SCARDHANDLE hCard)
{
	READER_CONTEXT * rContext = NULL;
	LONG rv;

	(void)RFReaderInfoById(hCard, &rContext);

	(void)SYS_MutexLock(&LockMutex);
	rv = RFCheckSharing(hCard);
	if (SCARD_S_SUCCESS == rv)
	{
		rContext->LockCount += 1;
		rContext->hLockId = hCard;
	}
	(void)SYS_MutexUnLock(&LockMutex);

	return rv;
}

LONG RFUnlockSharing(SCARDHANDLE hCard)
{
	READER_CONTEXT * rContext = NULL;
	LONG rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	(void)SYS_MutexLock(&LockMutex);
	rv = RFCheckSharing(hCard);
	if (SCARD_S_SUCCESS == rv)
	{
		if (rContext->LockCount > 0)
			rContext->LockCount -= 1;
		if (0 == rContext->LockCount)
			rContext->hLockId = 0;
	}
	(void)SYS_MutexUnLock(&LockMutex);

	return rv;
}

LONG RFUnlockAllSharing(SCARDHANDLE hCard)
{
	READER_CONTEXT * rContext = NULL;
	LONG rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	(void)SYS_MutexLock(&LockMutex);
	rv = RFCheckSharing(hCard);
	if (SCARD_S_SUCCESS == rv)
	{
		rContext->LockCount = 0;
		rContext->hLockId = 0;
	}
	(void)SYS_MutexUnLock(&LockMutex);

	return rv;
}

LONG RFInitializeReader(READER_CONTEXT * rContext)
{
	LONG rv;

	/* Spawn the event handler thread */
	Log3(PCSC_LOG_INFO, "Attempting startup of %s using %s",
		rContext->lpcReader, rContext->lpcLibrary);

#ifndef PCSCLITE_STATIC_DRIVER
	/* loads the library */
	rv = RFLoadReader(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "RFLoadReader failed: %X", rv);
		return rv;
	}

	/* binds the functions */
	rv = RFBindFunctions(rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "RFBindFunctions failed: %X", rv);
		(void)RFUnloadReader(rContext);
		return rv;
	}
#else
	/* define a fake vHandle. Can be any value except NULL */
	rContext->vHandle = RFInitializeReader;
#endif

	/* tries to open the port */
	rv = IFDOpenIFD(rContext);

	if (rv != IFD_SUCCESS)
	{
		Log3(PCSC_LOG_CRITICAL, "Open Port %X Failed (%s)",
			rContext->port, rContext->lpcDevice);
		(void)RFUnBindFunctions(rContext);
		(void)RFUnloadReader(rContext);
		if (IFD_NO_SUCH_DEVICE == rv)
			return SCARD_E_UNKNOWN_READER;
		else
			return SCARD_E_INVALID_TARGET;
	}

	return SCARD_S_SUCCESS;
}

LONG RFUnInitializeReader(READER_CONTEXT * rContext)
{
	Log2(PCSC_LOG_INFO, "Attempting shutdown of %s.",
		rContext->lpcReader);

	/* Close the port, unbind the functions, and unload the library */

	/*
	 * If the reader is getting uninitialized then it is being unplugged
	 * so I can't send a IFDPowerICC call to it
	 *
	 * IFDPowerICC( rContext, IFD_POWER_DOWN, Atr, &AtrLen );
	 */
	(void)IFDCloseIFD(rContext);
	(void)RFUnBindFunctions(rContext);
	(void)RFUnloadReader(rContext);

	return SCARD_S_SUCCESS;
}

SCARDHANDLE RFCreateReaderHandle(READER_CONTEXT * rContext)
{
	USHORT randHandle;

	/* Create a random handle with 16 bits check to see if it already is
	 * used. */
	/* FIXME: THIS IS NOT STRONG ENOUGH: A 128-bit token should be
	 * generated.  The client and server would associate token and hCard
	 * for authentication. */
	randHandle = SYS_RandomInt(10, 65000);

	int i;
again:
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			RDR_CLIHANDLES *currentHandle;
			list_t * l = &((sReadersContexts[i])->handlesList);

			list_iterator_start(l);
			while (list_iterator_hasnext(l))
			{
				currentHandle = list_iterator_next(l);
				if ((rContext->dwIdentity + randHandle) ==
					(currentHandle->hCard))
				{
					/* Get a new handle and loop again */
					randHandle = SYS_RandomInt(10, 65000);
					list_iterator_stop(l);
					goto again;
				}
			}
			list_iterator_stop(l);
		}
	}

	/* Once the for loop is completed w/o restart a good handle was
	 * found and the loop can be exited. */
	return rContext->dwIdentity + randHandle;
}

LONG RFFindReaderHandle(SCARDHANDLE hCard)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			RDR_CLIHANDLES * currentHandle;
			currentHandle = list_seek(&((sReadersContexts[i])->handlesList),
				&hCard);
			if (currentHandle != NULL)
				return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_INVALID_HANDLE;
}

LONG RFDestroyReaderHandle(/*@unused@*/ SCARDHANDLE hCard)
{
	(void)hCard;
	return SCARD_S_SUCCESS;
}

LONG RFAddReaderHandle(READER_CONTEXT * rContext, SCARDHANDLE hCard)
{
	int listLength, lrv;
	RDR_CLIHANDLES *newHandle;

	listLength = list_size(&(rContext->handlesList));

	/* Throttle the number of possible handles */
	if (listLength >= maxReaderHandles)
	{
		Log2(PCSC_LOG_CRITICAL,
			"Too many handles opened, exceeding configured max (%d)",
			maxReaderHandles);
		return SCARD_E_NO_MEMORY;
	}

	newHandle = malloc(sizeof(RDR_CLIHANDLES));
	if (NULL == newHandle)
	{
		Log1(PCSC_LOG_CRITICAL, "malloc failed");
		return SCARD_E_NO_MEMORY;
	}

	newHandle->hCard = hCard;
	newHandle->dwEventStatus = 0;

	lrv = list_append(&(rContext->handlesList), newHandle);
	if (lrv < 0)
	{
		free(newHandle);
		Log2(PCSC_LOG_CRITICAL, "list_append failed with return value: %X",
			lrv);
		return SCARD_E_NO_MEMORY;
	}
	return SCARD_S_SUCCESS;
}

LONG RFRemoveReaderHandle(READER_CONTEXT * rContext, SCARDHANDLE hCard)
{
	RDR_CLIHANDLES *currentHandle;
	int lrv;

	currentHandle = list_seek(&(rContext->handlesList), &hCard);
	if (NULL == currentHandle)
	{
		Log2(PCSC_LOG_CRITICAL, "list_seek failed to locate hCard=%X", hCard);
		return SCARD_E_INVALID_HANDLE;
	}

	lrv = list_delete(&(rContext->handlesList), currentHandle);
	if (lrv < 0)
		Log2(PCSC_LOG_CRITICAL,
			"list_delete failed with return value: %X", lrv);

	free(currentHandle);

	/* Not Found */
	return SCARD_S_SUCCESS;
}

LONG RFSetReaderEventState(READER_CONTEXT * rContext, DWORD dwEvent)
{
	/* Set all the handles for that reader to the event */
	int list_index, listSize;
	RDR_CLIHANDLES *currentHandle;

	listSize = list_size(&(rContext->handlesList));

	for (list_index = 0; list_index < listSize; list_index++)
	{
		currentHandle = list_get_at(&(rContext->handlesList), list_index);
		if (NULL == currentHandle)
		{
			Log2(PCSC_LOG_CRITICAL, "list_get_at failed at index %s",
				list_index);
			continue;
		}

		currentHandle->dwEventStatus = dwEvent;
	}

	if (SCARD_REMOVED == dwEvent)
	{
		/* unlock the card */
		rContext->hLockId = 0;
		rContext->LockCount = 0;
	}

	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderEventState(READER_CONTEXT * rContext, SCARDHANDLE hCard)
{
	LONG rv;
	RDR_CLIHANDLES *currentHandle;

	currentHandle = list_seek(&(rContext->handlesList), &hCard);
	if (NULL == currentHandle)
	{
		/* Not Found */
		Log2(PCSC_LOG_CRITICAL, "list_seek failed for hCard %X", hCard);
		return SCARD_E_INVALID_HANDLE;
	}

	switch(currentHandle->dwEventStatus)
	{
		case 0:
			rv = SCARD_S_SUCCESS;
			break;

		case SCARD_REMOVED:
			rv = SCARD_W_REMOVED_CARD;
			break;

		case SCARD_RESET:
			rv = SCARD_W_RESET_CARD;
			break;

		default:
			rv = SCARD_E_INVALID_VALUE;
	}

	return rv;
}

LONG RFClearReaderEventState(READER_CONTEXT * rContext, SCARDHANDLE hCard)
{
	RDR_CLIHANDLES *currentHandle;

	currentHandle = list_seek(&(rContext->handlesList), &hCard);
	if (NULL == currentHandle)
		/* Not Found */
		return SCARD_E_INVALID_HANDLE;

	currentHandle->dwEventStatus = 0;

	/* hCards should be unique so we
	 * should be able to return
	 * as soon as we have a hit */
	return SCARD_S_SUCCESS;
}

LONG RFCheckReaderStatus(READER_CONTEXT * rContext)
{
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN))
		return SCARD_E_READER_UNAVAILABLE;
	else
		return SCARD_S_SUCCESS;
}

void RFCleanupReaders(void)
{
	int i;

	Log1(PCSC_LOG_INFO, "entering cleaning function");
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if (sReadersContexts[i]->vHandle != 0)
		{
			LONG rv;
			char lpcStripReader[MAX_READERNAME];

			Log2(PCSC_LOG_INFO, "Stopping reader: %s",
				sReadersContexts[i]->lpcReader);

			strncpy(lpcStripReader, (sReadersContexts[i])->lpcReader,
				sizeof(lpcStripReader));
			/* strip the 6 last char ' 00 00' */
			lpcStripReader[strlen(lpcStripReader) - 6] = '\0';

			rv = RFRemoveReader(lpcStripReader, sReadersContexts[i]->port);

			if (rv != SCARD_S_SUCCESS)
				Log2(PCSC_LOG_ERROR, "RFRemoveReader error: 0x%08X", rv);
		}
	}
}

int RFStartSerialReaders(const char *readerconf)
{
	SerialReader *reader_list;
	int i, rv;

	/* remember the ocnfiguration filename for RFReCheckReaderConf() */
	ConfigFile = strdup(readerconf);

	rv = DBGetReaderList(readerconf, &reader_list);

	/* the list is empty */
	if (NULL == reader_list)
		return rv;

	for (i=0; reader_list[i].pcFriendlyname; i++)
	{
		int j;

		(void)RFAddReader(reader_list[i].pcFriendlyname,
			reader_list[i].channelId,
			reader_list[i].pcLibpath, reader_list[i].pcDevicename);

		/* update the ConfigFileCRC (this false "CRC" is very weak) */
		for (j=0; j<reader_list[i].pcFriendlyname[j]; j++)
			ConfigFileCRC += reader_list[i].pcFriendlyname[j];
		for (j=0; j<reader_list[i].pcLibpath[j]; j++)
			ConfigFileCRC += reader_list[i].pcLibpath[j];
		for (j=0; j<reader_list[i].pcDevicename[j]; j++)
			ConfigFileCRC += reader_list[i].pcDevicename[j];

		/* free strings allocated by DBGetReaderList() */
		free(reader_list[i].pcFriendlyname);
		free(reader_list[i].pcLibpath);
		free(reader_list[i].pcDevicename);
	}
	free(reader_list);

	return rv;
}

void RFReCheckReaderConf(void)
{
	SerialReader *reader_list;
	int i, crc;

	(void)DBGetReaderList(ConfigFile, &reader_list);

	/* the list is empty */
	if (NULL == reader_list)
		return;

	crc = 0;
	for (i=0; reader_list[i].pcFriendlyname; i++)
	{
		int j;

		/* calculate a local crc */
		for (j=0; j<reader_list[i].pcFriendlyname[j]; j++)
			crc += reader_list[i].pcFriendlyname[j];
		for (j=0; j<reader_list[i].pcLibpath[j]; j++)
			crc += reader_list[i].pcLibpath[j];
		for (j=0; j<reader_list[i].pcDevicename[j]; j++)
			crc += reader_list[i].pcDevicename[j];
	}

	/* cancel if the configuration file has been modified */
	if (crc != ConfigFileCRC)
	{
		Log2(PCSC_LOG_CRITICAL,
			"configuration file: %s has been modified. Recheck canceled",
			ConfigFile);
		return;
	}

	for (i=0; reader_list[i].pcFriendlyname; i++)
	{
		int r;
		char present = FALSE;

		Log2(PCSC_LOG_DEBUG, "refresh reader: %s",
			reader_list[i].pcFriendlyname);

		/* is the reader already present? */
		for (r = 0; r < PCSCLITE_MAX_READERS_CONTEXTS; r++)
		{
			if (sReadersContexts[r]->vHandle != 0)
			{
				char lpcStripReader[MAX_READERNAME];
				int tmplen;

				/* get the reader name without the reader and slot numbers */
				strncpy(lpcStripReader, sReadersContexts[i]->lpcReader,
					sizeof(lpcStripReader));
				tmplen = strlen(lpcStripReader);
				lpcStripReader[tmplen - 6] = 0;

				if ((strcmp(reader_list[i].pcFriendlyname, lpcStripReader) == 0)
					&& (reader_list[r].channelId == sReadersContexts[i]->port))
				{
					DWORD dwStatus = 0, dwAtrLen = 0;
					UCHAR ucAtr[MAX_ATR_SIZE];

					/* the reader was already started */
					present = TRUE;

					/* verify the reader is still connected */
					if (IFDStatusICC(sReadersContexts[r], &dwStatus, ucAtr,
						&dwAtrLen) != SCARD_S_SUCCESS)
					{
						Log2(PCSC_LOG_INFO, "Reader %s disappeared",
							reader_list[i].pcFriendlyname);
						(void)RFRemoveReader(reader_list[i].pcFriendlyname,
							reader_list[r].channelId);
					}
				}
			}
		}

		/* the reader was not present */
		if (!present)
			/* we try to add it */
			(void)RFAddReader(reader_list[i].pcFriendlyname,
				reader_list[i].channelId, reader_list[i].pcLibpath,
				reader_list[i].pcDevicename);

		/* free strings allocated by DBGetReaderList() */
		free(reader_list[i].pcFriendlyname);
		free(reader_list[i].pcLibpath);
		free(reader_list[i].pcDevicename);
	}
	free(reader_list);
}

#if 0
void RFSuspendAllReaders(void)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((sReadersContexts[i])->vHandle != 0)
		{
			(void)EHDestroyEventHandler(sReadersContexts[i]);
			(void)IFDCloseIFD(sReadersContexts[i]);
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
		if ( ((sReadersContexts[i])->vHandle != 0) &&
			((sReadersContexts[i])->pthThread == 0) )
		{
			int j;

			for (j=0; j < i; j++)
			{
				if (((sReadersContexts[j])->vHandle == (sReadersContexts[i])->vHandle)&&
					((sReadersContexts[j])->port == (sReadersContexts[i])->port))
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
				Log3(PCSC_LOG_ERROR, "Open Port %X Failed (%s)",
					(sReadersContexts[i])->port, (sReadersContexts[i])->lpcDevice);
			}

			(void)EHSpawnEventHandler(sReadersContexts[i], NULL);
			(void)RFSetReaderEventState(sReadersContexts[i], SCARD_RESET);
		}
	}
}
#endif

