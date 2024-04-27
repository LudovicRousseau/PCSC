/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2023
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
 * @brief This handles card insertion/removal events, updates ATR,
 * protocol, and status information.
 */

#ifndef __eventhandler_h__
#define __eventhandler_h__

#include <stdint.h>

#include "pcsclite.h"
#include "readerfactory.h"
#include "wintypes.h"

	/**
	 * Define an exported public reader state structure so each
	 * application gets instant notification of changes in state.
	 */
	typedef struct pubReaderStatesList
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

	LONG EHTryToUnregisterClientForEvent(int32_t filedes);
	LONG EHRegisterClientForEvent(int32_t filedes);
	LONG EHUnregisterClientForEvent(int32_t filedes);
	void EHSignalEventToClients(void);
	LONG EHInitializeEventStructures(void);
	LONG EHDeinitializeEventStructures(void);
	LONG EHSpawnEventHandler(READER_CONTEXT *);
	void EHDestroyEventHandler(READER_CONTEXT *);

/** One application is using the reader */
#define PCSCLITE_SHARING_LAST_CONTEXT       1
/** No application is using the reader */
#define PCSCLITE_SHARING_NO_CONTEXT         0
/** Reader used in exclusive mode */
#define PCSCLITE_SHARING_EXCLUSIVE_CONTEXT -1

/** Special value to indicate that power up has not yet happen
 * This is used to auto start mode to wait until the reader is
 * ready and the (possible) card has been powered up */
#define READER_NOT_INITIALIZED (MAX_ATR_SIZE+1)

#endif							/* __eventhandler_h__ */
