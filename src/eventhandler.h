/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles card insertion/removal events, updates ATR,
 * protocol, and status information.
 */

#ifndef __eventhandler_h__
#define __eventhandler_h__

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Define an exported public reader state structure so each
	 * application gets instant notification of changes in state.
	 */
	typedef struct pubReaderStatesList
	{
		char readerName[MAX_READERNAME]; /**< reader name */
		uint32_t eventCounter; /**< number of card events */
		uint32_t readerState; /**< SCARD_* bit field */
		int32_t readerSharing; /**< PCSCLITE_SHARING_* sharing status */

		UCHAR cardAtr[MAX_ATR_SIZE]; /**< ATR */
		uint32_t cardAtrLength; /**< ATR length */
		uint32_t cardProtocol; /**< SCARD_PROTOCOL_* value */
	}
	READER_STATE;

	LONG EHTryToUnregisterClientForEvent(int32_t filedes);
	LONG EHRegisterClientForEvent(int32_t filedes);
	LONG EHUnregisterClientForEvent(int32_t filedes); 
	LONG EHSignalEventToClients(void);
	LONG EHInitializeEventStructures(void);
	LONG EHSpawnEventHandler(READER_CONTEXT *);
	LONG EHDestroyEventHandler(READER_CONTEXT *);

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

#ifdef __cplusplus
}
#endif

#endif							/* __eventhandler_h__ */
