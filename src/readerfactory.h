/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This keeps track of a list of currently available reader structures.
 */

#ifndef __readerfactory_h__
#define __readerfactory_h__

#include <inttypes.h>
#include <pthread.h>

#include "ifdhandler.h"
#include "pcscd.h"
#include "simclist.h"

	typedef struct
	{
		char *pcFriendlyname;	/**< FRIENDLYNAME */
		char *pcDevicename;		/**< DEVICENAME */
		char *pcLibpath;		/**< LIBPATH */
		int channelId;		/**< CHANNELID */
	} SerialReader;

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

	struct RdrCliHandles
	{
		SCARDHANDLE hCard;		/**< hCard for this connection */
		DWORD dwEventStatus;	/**< Recent event that must be sent */
	};

	typedef struct RdrCliHandles RDR_CLIHANDLES;

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

		LPVOID vHandle;			/**< Dlopen handle */
		int version;			/**< IFD Handler version number */
		int port;				/**< Port ID */
		int slot;				/**< Current Reader Slot */
		SCARDHANDLE hLockId;	/**< Lock Id */
		int LockCount;			/**< number of recursive locks */
		int32_t contexts;		/**< Number of open contexts */
		int * pFeeds;			/**< Number of shared client to lib */
		int * pMutex;			/**< Number of client to mutex */
		int powerState;			/**< auto power off state */
		pthread_mutex_t powerState_lock;	/**< powerState mutex */

		struct pubReaderStatesList *readerState; /**< link to the reader state */
		/* we can't use READER_STATE * here since eventhandler.h can't be
		 * included because of circular dependencies */
	};

	typedef struct ReaderContext READER_CONTEXT;

	LONG RFAllocateReaderSpace(unsigned int);
	LONG RFAddReader(const char *, int, const char *, const char *);
	LONG RFRemoveReader(const char *, int);
	LONG RFSetReaderName(READER_CONTEXT *, const char *, const char *, int);
	LONG RFReaderInfo(const char *, /*@out@*/ struct ReaderContext **);
	LONG RFReaderInfoNamePort(int, const char *, /*@out@*/ struct ReaderContext **);
	LONG RFReaderInfoById(SCARDHANDLE, /*@out@*/ struct ReaderContext **);
	LONG RFCheckSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFLockSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFUnlockSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFUnlockAllSharing(SCARDHANDLE, READER_CONTEXT *);
	LONG RFLoadReader(READER_CONTEXT *);
	LONG RFBindFunctions(READER_CONTEXT *);
	LONG RFUnBindFunctions(READER_CONTEXT *);
	LONG RFUnloadReader(READER_CONTEXT *);
	LONG RFInitializeReader(READER_CONTEXT *);
	LONG RFUnInitializeReader(READER_CONTEXT *);
	SCARDHANDLE RFCreateReaderHandle(READER_CONTEXT *);
	LONG RFDestroyReaderHandle(SCARDHANDLE hCard);
	LONG RFAddReaderHandle(READER_CONTEXT *, SCARDHANDLE);
	LONG RFRemoveReaderHandle(READER_CONTEXT *, SCARDHANDLE);
	LONG RFSetReaderEventState(READER_CONTEXT *, DWORD);
	LONG RFCheckReaderEventState(READER_CONTEXT *, SCARDHANDLE);
	LONG RFClearReaderEventState(READER_CONTEXT *, SCARDHANDLE);
	LONG RFCheckReaderStatus(READER_CONTEXT *);
	void RFCleanupReaders(void);
	void RFWaitForReaderInit(void);
	int RFStartSerialReaders(const char *readerconf);
	void RFReCheckReaderConf(void);

#endif
