/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2026-2026
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief define structures to represent a reader
 */

#ifndef __READERS_H__
#define __READERS_H__

#include <inttypes.h>
#include <pthread.h>

#include "ifdhandler.h"
#include "simclist.h"

/**
 * Define an exported public reader state structure so each
 * application gets instant notification of changes in state.
 */
typedef struct pubReaderState
{
	char readerName[MAX_READERNAME]; /**< reader name */
	uint32_t eventCounter; /**< number of card events */
	uint32_t readerState; /**< SCARD_* bit field */
	_Atomic int32_t readerSharing; /**< PCSCLITE_SHARING_* sharing status */

	UCHAR cardAtr[MAX_ATR_SIZE]; /**< ATR */
	_Atomic uint32_t cardAtrLength; /**< ATR length */
	uint32_t cardProtocol; /**< SCARD_PROTOCOL_* value */
}
READER_STATE;

struct FctMap_V2
{
	/* shared with API 3.0 */
	RESPONSECODE (*pvfCreateChannel)(DWORD, DWORD);
	RESPONSECODE (*pvfCloseChannel)(DWORD);
	RESPONSECODE (*pvfGetCapabilities)(DWORD, DWORD, PDWORD, PUCHAR);
	RESPONSECODE (*pvfSetCapabilities)(DWORD, DWORD, DWORD, PUCHAR);
	RESPONSECODE (*pvfSetProtocolParameters)(DWORD, DWORD, UCHAR, UCHAR,
		UCHAR, UCHAR);
	RESPONSECODE (*pvfPowerICC)(DWORD, DWORD, PUCHAR, PDWORD);
	RESPONSECODE (*pvfTransmitToICC)(DWORD, SCARD_IO_HEADER, PUCHAR,
		DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER);
	RESPONSECODE (*pvfICCPresence)(DWORD);

	/* API v2.0 only */
	RESPONSECODE (*pvfControl)(DWORD, PUCHAR, DWORD, PUCHAR, PDWORD);
};

typedef struct FctMap_V2 FCT_MAP_V2;

struct FctMap_V3
{
	/* the common fields SHALL be in the same order as in FctMap_V2 */
	RESPONSECODE (*pvfCreateChannel)(DWORD, DWORD);
	RESPONSECODE (*pvfCloseChannel)(DWORD);
	RESPONSECODE (*pvfGetCapabilities)(DWORD, DWORD, PDWORD, PUCHAR);
	RESPONSECODE (*pvfSetCapabilities)(DWORD, DWORD, DWORD, PUCHAR);
	RESPONSECODE (*pvfSetProtocolParameters)(DWORD, DWORD, UCHAR, UCHAR,
			UCHAR, UCHAR);
	RESPONSECODE (*pvfPowerICC)(DWORD, DWORD, PUCHAR, PDWORD);
	RESPONSECODE (*pvfTransmitToICC)(DWORD, SCARD_IO_HEADER, PUCHAR,
		DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER);
	RESPONSECODE (*pvfICCPresence)(DWORD);

	/* API V3.0 only */
	RESPONSECODE (*pvfControl)(DWORD, DWORD, LPCVOID, DWORD, LPVOID,
		DWORD, LPDWORD);
	RESPONSECODE (*pvfCreateChannelByName)(DWORD, LPSTR);
};

typedef struct FctMap_V3 FCT_MAP_V3;

struct ReaderContext
{
	char *library;	/**< Library Path */
	char *device;	/**< Device Name */
	pthread_t pthThread;	/**< Event polling thread */
	RESPONSECODE (*pthCardEvent)(DWORD, int);	/**< Card Event sync */
	pthread_mutex_t *mMutex;	/**< Mutex for this connection */
	list_t handlesList;
	pthread_mutex_t handlesList_lock;	/**< lock for the above list */
									 /**< Structure of connected handles */
	union
	{
		FCT_MAP_V2 psFunctions_v2;	/**< API V2.0 */
		FCT_MAP_V3 psFunctions_v3;	/**< API V3.0 */
	} psFunctions;	/**< driver functions */

	_Atomic LPVOID vHandle;			/**< Dlopen handle */
	int version;			/**< IFD Handler version number */
	int port;				/**< Port ID */
	int slot;				/**< Current Reader Slot */
	_Atomic SCARDHANDLE hLockId;	/**< Lock Id */
	_Atomic int LockCount;			/**< number of recursive locks */
	_Atomic int32_t contexts;		/**< Number of open contexts */
	int * pFeeds;			/**< Number of shared client to lib */
	int * pMutex;			/**< Number of client to mutex */
	int powerState;			/**< auto power off state */
	pthread_mutex_t powerState_lock;	/**< powerState mutex */
	_Atomic int reference;			/**< number of users of the structure */

	READER_STATE readerState; /**< reader state */
};

typedef struct ReaderContext READER_CONTEXT;

#endif
