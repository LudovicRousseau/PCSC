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
 * @brief This keeps track of a list of currently available reader structures.
 */

#ifndef __readerfactory_h__
#define __readerfactory_h__

#include <inttypes.h>

#include "thread_generic.h"
#include "ifdhandler.h"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct
	{
		char *pcFriendlyname;
		char *pcDevicename;
		char *pcLibpath;
		int dwChannelId;
	} SerialReader;

	struct FctMap_V1
	{
		RESPONSECODE (*pvfCreateChannel)(DWORD);
		RESPONSECODE (*pvfCloseChannel)(void);
		RESPONSECODE (*pvfGetCapabilities)(DWORD, PUCHAR);
		RESPONSECODE (*pvfSetCapabilities)(DWORD, PUCHAR);
		RESPONSECODE (*pvfSetProtocolParameters)(DWORD, UCHAR, UCHAR, UCHAR,
			UCHAR);
		RESPONSECODE (*pvfPowerICC)(DWORD);
		RESPONSECODE (*pvfTransmitToICC)(SCARD_IO_HEADER, PUCHAR, DWORD,
			PUCHAR, PDWORD, PSCARD_IO_HEADER);
		RESPONSECODE (*pvfICCPresence)(void);
	};

	typedef struct FctMap_V1 FCT_MAP_V1, *PFCT_MAP_V1;

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

	typedef struct FctMap_V2 FCT_MAP_V2, *PFCT_MAP_V2;

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

	typedef struct FctMap_V3 FCT_MAP_V3, *PFCT_MAP_V3;

	/*
	 * The following is not currently used but in place if needed
	 */

	struct RdrCapabilities
	{
		DWORD dwAsynch_Supported;	/* Asynchronous Support */
		DWORD dwDefault_Clock;	/* Default Clock Rate */
		DWORD dwMax_Clock;		/* Max Clock Rate */
		DWORD dwDefault_Data_Rate;	/* Default Data Rate */
		DWORD dwMax_Data_Rate;	/* Max Data Rate */
		DWORD dwMax_IFSD;		/* Maximum IFSD Size */
		DWORD dwSynch_Supported;	/* Synchronous Support */
		DWORD dwPower_Mgmt;		/* Power Mgmt Features */
		DWORD dwCard_Auth_Devices;	/* Card Auth Devices */
		DWORD dwUser_Auth_Device;	/* User Auth Devices */
		DWORD dwMechanics_Supported;	/* Machanics Supported */
		DWORD dwVendor_Features;	/* User Defined.  */
	};

	typedef struct RdrCapabilities RDR_CAPABILITIES, *PRDR_CAPABILITIES;

	struct ProtOptions
	{
		DWORD dwProtocol_Type;	/* Protocol Type */
		DWORD dwCurrent_Clock;	/* Current Clock */
		DWORD dwCurrent_F;		/* Current F */
		DWORD dwCurrent_D;		/* Current D */
		DWORD dwCurrent_N;		/* Current N */
		DWORD dwCurrent_W;		/* Current W */
		DWORD dwCurrent_IFSC;	/* Current IFSC */
		DWORD dwCurrent_IFSD;	/* Current IFSD */
		DWORD dwCurrent_BWT;	/* Current BWT */
		DWORD dwCurrent_CWT;	/* Current CWT */
		DWORD dwCurrent_EBC;	/* Current EBC */
	};

	typedef struct ProtOptions PROT_OPTIONS, *PPROT_OPTIONS;

	struct RdrCliHandles
	{
		SCARDHANDLE hCard;		/* hCard for this connection */
		DWORD dwEventStatus;	/* Recent event that must be sent */
	};

	typedef struct RdrCliHandles RDR_CLIHANDLES, *PRDR_CLIHANDLES;

	struct ReaderContext
	{
		char lpcReader[MAX_READERNAME];	/* Reader Name */
		char lpcLibrary[MAX_LIBNAME];	/* Library Path */
		char lpcDevice[MAX_DEVICENAME];	/* Device Name */
		PCSCLITE_THREAD_T pthThread;	/* Event polling thread */
		PCSCLITE_MUTEX_T mMutex;	/* Mutex for this connection */
		RDR_CLIHANDLES psHandles[PCSCLITE_MAX_READER_CONTEXT_CHANNELS];
                                         /* Structure of connected handles */
		union
		{
			FCT_MAP_V1 psFunctions_v1;	/* API V1.0 */
			FCT_MAP_V2 psFunctions_v2;	/* API V2.0 */
			FCT_MAP_V3 psFunctions_v3;	/* API V3.0 */
		} psFunctions;

		LPVOID vHandle;			/* Dlopen handle */
		DWORD dwVersion;		/* IFD Handler version number */
		DWORD dwPort;			/* Port ID */
		DWORD dwSlot;			/* Current Reader Slot */
		DWORD dwBlockStatus;	/* Current blocking status */
		DWORD dwLockId;			/* Lock Id */
		DWORD dwIdentity;		/* Shared ID High Nibble */
		int32_t dwContexts;		/* Number of open contexts */
		PDWORD pdwFeeds;		/* Number of shared client to lib */
		PDWORD pdwMutex;		/* Number of client to mutex */

		struct pubReaderStatesList *readerState; /* link to the reader state */
		/* we can't use PREADER_STATE here since eventhandler.h can't be
		 * included because of circular dependencies */

		/* these structures are unused */
#if 0
		RDR_CAPABILITIES psCapabilites;	/* Structure of reader
						   capabilities */
		PROT_OPTIONS psProtOptions;	/* Structure of protocol options */
#endif
	};

	typedef struct ReaderContext READER_CONTEXT, *PREADER_CONTEXT;

	LONG RFAllocateReaderSpace(void);
	LONG RFAddReader(LPSTR, DWORD, LPSTR, LPSTR);
	LONG RFRemoveReader(LPSTR, DWORD);
	LONG RFSetReaderName(PREADER_CONTEXT, LPSTR, LPSTR, DWORD, DWORD);
	LONG RFListReaders(LPSTR, LPDWORD);
	LONG RFReaderInfo(LPSTR, struct ReaderContext **);
	LONG RFReaderInfoNamePort(DWORD, LPSTR, struct ReaderContext **);
	LONG RFReaderInfoById(DWORD, struct ReaderContext **);
	LONG RFCheckSharing(DWORD);
	LONG RFLockSharing(DWORD);
	LONG RFUnlockSharing(DWORD);
	LONG RFUnblockReader(PREADER_CONTEXT);
	LONG RFUnblockContext(SCARDCONTEXT);
	LONG RFLoadReader(PREADER_CONTEXT);
	LONG RFBindFunctions(PREADER_CONTEXT);
	LONG RFUnBindFunctions(PREADER_CONTEXT);
	LONG RFUnloadReader(PREADER_CONTEXT);
	LONG RFInitializeReader(PREADER_CONTEXT);
	LONG RFUnInitializeReader(PREADER_CONTEXT);
	SCARDHANDLE RFCreateReaderHandle(PREADER_CONTEXT);
	LONG RFDestroyReaderHandle(SCARDHANDLE hCard);
	LONG RFAddReaderHandle(PREADER_CONTEXT, SCARDHANDLE);
	LONG RFFindReaderHandle(SCARDHANDLE);
	LONG RFRemoveReaderHandle(PREADER_CONTEXT, SCARDHANDLE);
	LONG RFSetReaderEventState(PREADER_CONTEXT, DWORD);
	LONG RFCheckReaderEventState(PREADER_CONTEXT, SCARDHANDLE);
	LONG RFClearReaderEventState(PREADER_CONTEXT, SCARDHANDLE);
	LONG RFCheckReaderStatus(PREADER_CONTEXT);
	void RFCleanupReaders(int);
	int RFStartSerialReaders(char *readerconf);
	void RFReCheckReaderConf(void);
	void RFSuspendAllReaders(void);
	void RFAwakeAllReaders(void);

#ifdef __cplusplus
}
#endif

#endif
