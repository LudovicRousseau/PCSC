/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
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
		int32_t readerID;
		char readerName[MAX_READERNAME];
		uint32_t readerState;
		int32_t readerSharing;

		UCHAR cardAtr[MAX_ATR_SIZE];
		uint32_t cardAtrLength;
		uint32_t cardProtocol;
	}
	READER_STATE, *PREADER_STATE;

	LONG EHTryToUnregisterClientForEvent(int32_t filedes);
	LONG EHRegisterClientForEvent(int32_t filedes);
	LONG EHUnregisterClientForEvent(int32_t filedes); 
	LONG EHSignalEventToClients(void);
	LONG EHInitializeEventStructures(void);
	LONG EHSpawnEventHandler(PREADER_CONTEXT,
		/*@null@*/ RESPONSECODE (*)(DWORD));
	LONG EHDestroyEventHandler(PREADER_CONTEXT);

/** Some defines for context stack. */
#define SCARD_LAST_CONTEXT       1
/** Some defines for context stack. */
#define SCARD_NO_CONTEXT         0
/** Some defines for context stack. */
#define SCARD_EXCLUSIVE_CONTEXT -1

#ifdef __cplusplus
}
#endif

#endif							/* __eventhandler_h__ */
