/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @defgroup API
 * @brief This handles smartcard reader communications and
 * forwarding requests over message queues.
 *
 * Here is exposed the API for client applications.
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <sys/time.h>

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debug.h"
#include "thread_generic.h"
#include "strlcpycat.h"

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"
#include "winscard_msg.h"
#include "utils.h"

/** used for backward compatibility */
#define SCARD_PROTOCOL_ANY_OLD	0x1000

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* r = a - b */
static long int time_sub(struct timeval *a, struct timeval *b)
{
	struct timeval r;
	r.tv_sec = a -> tv_sec - b -> tv_sec;
	r.tv_usec = a -> tv_usec - b -> tv_usec;
	if (r.tv_usec < 0)
	{
		r.tv_sec--;
		r.tv_usec += 1000000;
	}

	return r.tv_sec * 1000000 + r.tv_usec;
} /* time_sub */


#undef DO_PROFILE
#ifdef DO_PROFILE

#define PROFILE_FILE "/tmp/pcsc_profile"
#include <stdio.h>
#include <sys/time.h>

struct timeval profile_time_start;
FILE *fd;
char profile_tty;
char fct_name[100];

#define PROFILE_START profile_start(__FUNCTION__);
#define PROFILE_END(rv) profile_end(__FUNCTION__, rv);

static void profile_start(const char *f)
{
	static char initialized = FALSE;

	if (!initialized)
	{
		char filename[80];

		initialized = TRUE;
		sprintf(filename, "%s-%d", PROFILE_FILE, getuid());
		fd = fopen(filename, "a+");
		if (NULL == fd)
		{
			fprintf(stderr, "\33[01;31mCan't open %s: %s\33[0m\n",
				PROFILE_FILE, strerror(errno));
			exit(-1);
		}
		fprintf(fd, "\nStart a new profile\n");

		if (isatty(fileno(stderr)))
			profile_tty = TRUE;
		else
			profile_tty = FALSE;
	}

	/* PROFILE_END was not called before? */
	if (profile_tty && fct_name[0])
		printf("\33[01;34m WARNING: %s starts before %s finishes\33[0m\n",
			f, fct_name);

	strlcpy(fct_name, f, sizeof(fct_name));

	gettimeofday(&profile_time_start, NULL);
} /* profile_start */

static void profile_end(const char *f, LONG rv)
{
	struct timeval profile_time_end;
	long d;

	gettimeofday(&profile_time_end, NULL);
	d = time_sub(&profile_time_end, &profile_time_start);

	if (profile_tty)
	{
		if (fct_name[0])
		{
			if (strncmp(fct_name, f, sizeof(fct_name)))
				printf("\33[01;34m WARNING: %s ends before %s\33[0m\n",
						f, fct_name);
		}
		else
			printf("\33[01;34m WARNING: %s ends but we lost its start\33[0m\n",
				f);

		/* allow to detect missing PROFILE_END calls */
		fct_name[0] = '\0';

		if (rv != SCARD_S_SUCCESS)
			fprintf(stderr,
				"\33[01;31mRESULT %s \33[35m%ld \33[34m0x%08lX %s\33[0m\n",
				f, d, rv, pcsc_stringify_error(rv));
		else
			fprintf(stderr, "\33[01;31mRESULT %s \33[35m%ld\33[0m\n", f, d);
	}
	fprintf(fd, "%s %ld\n", f, d);
	fflush(fd);
} /* profile_end */

#else
#define PROFILE_START
#define PROFILE_END(rv)
#endif

/**
 * Represents an Application Context Channel.
 * A channel belongs to an Application Context (\c _psContextMap).
 */
struct _psChannelMap
{
	SCARDHANDLE hCard;
	LPSTR readerName;
};

typedef struct _psChannelMap CHANNEL_MAP, *PCHANNEL_MAP;

/**
 * @brief Represents the an Application Context on the Client side.
 *
 * An Application Context contains Channels (\c _psChannelMap).
 */
static struct _psContextMap
{
	DWORD dwClientID;				/**< Client Connection ID */
	SCARDCONTEXT hContext;			/**< Application Context ID */
	DWORD contextBlockStatus;
	PCSCLITE_MUTEX_T mMutex;		/**< Mutex for this context */
	CHANNEL_MAP psChannelMap[PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS];
} psContextMap[PCSCLITE_MAX_APPLICATION_CONTEXTS];

/**
 * Make sure the initialization code is executed only once.
 */
static short isExecuted = 0;


/**
 * creation time of pcscd PCSCLITE_PUBSHM_FILE file
 */
static time_t daemon_ctime = 0;
static pid_t daemon_pid = 0;
/**
 * PID of the client application.
 * Used to detect fork() and disable handles in the child process
 */
static pid_t client_pid = 0;

/**
 * Memory mapped address used to read status information about the readers.
 * Each element in the vector \ref readerStates makes references to a part of
 * the memory mapped.
 */
static int mapAddr = 0;

/**
 * Ensure that some functions be accessed in thread-safe mode.
 * These function's names finishes with "TH".
 */
static PCSCLITE_MUTEX clientMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Pointers to a memory mapped area used to read status information about the
 * readers.
 * Each element in the vector \ref readerStates makes references to a part of
 * the memory mapped \ref mapAddr.
 */
static PREADER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

PCSC_API SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };	/**< Protocol Control Information for T=0 */
PCSC_API SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };	/**< Protocol Control Information for T=1 */
PCSC_API SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };	/**< Protocol Control Information for raw access */


static LONG SCardAddContext(SCARDCONTEXT, DWORD);
static LONG SCardGetContextIndice(SCARDCONTEXT);
static LONG SCardGetContextIndiceTH(SCARDCONTEXT);
static LONG SCardRemoveContext(SCARDCONTEXT);
static LONG SCardCleanContext(LONG indice);

static LONG SCardAddHandle(SCARDHANDLE, DWORD, LPCSTR);
static LONG SCardGetIndicesFromHandle(SCARDHANDLE, PDWORD, PDWORD);
static LONG SCardGetIndicesFromHandleTH(SCARDHANDLE, PDWORD, PDWORD);
static LONG SCardRemoveHandle(SCARDHANDLE);

static LONG SCardGetSetAttrib(SCARDHANDLE hCard, int command, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen);

void DESTRUCTOR SCardUnload(void);

/*
 * Thread safety functions
 */
/**
 * @brief This function locks a mutex so another thread must wait to use this
 * function.
 *
 * Wrapper to the function SYS_MutexLock().
 */
inline static LONG SCardLockThread(void)
{
	return SYS_MutexLock(&clientMutex);
}

/**
 * @brief This function unlocks a mutex so another thread may use the client.
 *
 * Wrapper to the function SYS_MutexUnLock().
 */
inline static LONG SCardUnlockThread(void)
{
	return SYS_MutexUnLock(&clientMutex);
}

static LONG SCardEstablishContextTH(DWORD, LPCVOID, LPCVOID, LPSCARDCONTEXT);

/**
 * @brief Creates an Application Context to the PC/SC Resource Manager.

 * This must be the first function called in a PC/SC application.
 *
 * @ingroup API
 * @param[in] dwScope Scope of the establishment.
 * This can either be a local or remote connection.
 * - \ref SCARD_SCOPE_USER - Not used.
 * - \ref SCARD_SCOPE_TERMINAL - Not used.
 * - \ref SCARD_SCOPE_GLOBAL - Not used.
 * - \ref SCARD_SCOPE_SYSTEM - Services on the local machine.
 * @param[in] pvReserved1 Reserved for future use.
 * @param[in] pvReserved2 Reserved for future use.
 * @param[out] phContext Returned Application Context.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_PARAMETER \p phContext is null (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid scope type passed (\ref SCARD_E_INVALID_VALUE )
 * @retval SCARD_E_NO_MEMORY There is no free slot to store \p hContext (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * @endcode
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;

	PROFILE_START

	/* Check if the server is running */
	rv = SCardCheckDaemonAvailability();
	if (SCARD_E_INVALID_HANDLE == rv)
		/* we reconnected to a daemon or we got called from a forked child */
		rv = SCardCheckDaemonAvailability();

	if (rv != SCARD_S_SUCCESS)
		return rv;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief Creates a communication context to the PC/SC Resource
 * Manager.
 *
 * This function should not be called directly. Instead, the thread-safe
 * function SCardEstablishContext() should be called.
 *
 * @param[in] dwScope Scope of the establishment.
 * This can either be a local or remote connection.
 * - \ref SCARD_SCOPE_USER - Not used.
 * - \ref SCARD_SCOPE_TERMINAL - Not used.
 * - \ref SCARD_SCOPE_GLOBAL - Not used.
 * - \ref SCARD_SCOPE_SYSTEM - Services on the local machine.
 * @param[in] pvReserved1 Reserved for future use. Can be used for remote connection.
 * @param[in] pvReserved2 Reserved for future use.
 * @param[out] phContext Returned reference to this connection.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_PARAMETER \p phContext is null. (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid scope type passed (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_MEMORY There is no free slot to store \p hContext (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 */
static LONG SCardEstablishContextTH(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;
	int i;
	establish_struct scEstablishStruct;
	sharedSegmentMsg msgStruct;
	uint32_t dwClientID = 0;

	if (phContext == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phContext = 0;

	/*
	 * Do this only once:
	 * - Initialize debug of need.
	 * - Set up the memory mapped structures for reader states.
	 * - Allocate each reader structure.
	 * - Initialize context struct.
	 */
	if (isExecuted == 0)
	{
		int pageSize;

		/*
		 * Do any system initilization here
		 */
		SYS_Initialize();

		/*
		 * Set up the memory mapped reader stats structures
		 */
		mapAddr = SYS_OpenFile(PCSCLITE_PUBSHM_FILE, O_RDONLY, 0);
		if (mapAddr < 0)
		{
			Log3(PCSC_LOG_CRITICAL, "Cannot open public shared file %s: %s",
				PCSCLITE_PUBSHM_FILE, strerror(errno));
			return SCARD_E_NO_SERVICE;
		}

		/* close on exec so that child processes do not inherits the file
		 * descriptor. The child process will call SCardEstablishContext()
		 * if needed. */
		fcntl(mapAddr, F_SETFD, FD_CLOEXEC);

		pageSize = SYS_GetPageSize();

		/*
		 * Allocate each reader structure in the memory map
		 */
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			readerStates[i] =
				(PREADER_STATE)SYS_PublicMemoryMap(sizeof(READER_STATE),
				mapAddr, (i * pageSize));
			if (readerStates[i] == NULL)
			{
				Log2(PCSC_LOG_CRITICAL, "Cannot public memory map: %s",
					strerror(errno));
				SYS_CloseFile(mapAddr);	/* Close the memory map file */
				return SCARD_F_INTERNAL_ERROR;
			}
		}

		/*
		 * Initializes the application contexts and all channels for each one
		 */
		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
		{
			int j;

			/*
			 * Initially set the context struct to zero
			 */
			psContextMap[i].dwClientID = 0;
			psContextMap[i].hContext = 0;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			psContextMap[i].mMutex = NULL;

			for (j = 0; j < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; j++)
			{
				/*
				 * Initially set the hcard structs to zero
				 */
				psContextMap[i].psChannelMap[j].hCard = 0;
				psContextMap[i].psChannelMap[j].readerName = NULL;
			}
		}

	}

	/*
	 * Is there a free slot for this connection ?
	 */

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].dwClientID == 0)
			break;
	}

	if (i == PCSCLITE_MAX_APPLICATION_CONTEXTS)
	{
		return SCARD_E_NO_MEMORY;
	}

	/* Establishes a connection to the server */
	if (SHMClientSetupSession(&dwClientID) != 0)
	{
		SYS_CloseFile(mapAddr);
		return SCARD_E_NO_SERVICE;
	}

	{	/* exchange client/server protocol versions */
		version_struct *veStr;

		memset(&msgStruct, 0, sizeof(msgStruct));
		msgStruct.mtype = CMD_VERSION;
		msgStruct.user_id = SYS_GetUID();
		msgStruct.group_id = SYS_GetGID();
		msgStruct.command = 0;
		msgStruct.date = time(NULL);

		veStr = (version_struct *) msgStruct.data;
		veStr->major = PROTOCOL_VERSION_MAJOR;
		veStr->minor = PROTOCOL_VERSION_MINOR;

		if (-1 == SHMMessageSend(&msgStruct, sizeof(msgStruct), dwClientID,
			PCSCLITE_MCLIENT_ATTEMPTS))
			return SCARD_E_NO_SERVICE;

		/*
		 * Read a message from the server
		 */
		if (-1 == SHMMessageReceive(&msgStruct, sizeof(msgStruct), dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS))
		{
			Log1(PCSC_LOG_CRITICAL, "Your pcscd is too old and does not support CMD_VERSION");
			return SCARD_F_COMM_ERROR;
		}

		Log3(PCSC_LOG_INFO, "Server is protocol version %d:%d",
			veStr->major, veStr->minor);

		if (veStr->rv != SCARD_S_SUCCESS)
			return veStr->rv;

		isExecuted = 1;
	}


	/*
	 * Try to establish an Application Context with the server
	 */
	scEstablishStruct.dwScope = dwScope;
	scEstablishStruct.phContext = 0;
	scEstablishStruct.rv = SCARD_S_SUCCESS;

	rv = WrapSHMWrite(SCARD_ESTABLISH_CONTEXT, dwClientID,
		sizeof(scEstablishStruct), PCSCLITE_MCLIENT_ATTEMPTS,
		(void *) &scEstablishStruct);

	if (rv == -1)
		return SCARD_E_NO_SERVICE;

	/*
	 * Read the response from the server
	 */
	rv = SHMClientRead(&msgStruct, dwClientID, PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
		return SCARD_F_COMM_ERROR;

	memcpy(&scEstablishStruct, &msgStruct.data, sizeof(scEstablishStruct));

	if (scEstablishStruct.rv != SCARD_S_SUCCESS)
		return scEstablishStruct.rv;

	*phContext = scEstablishStruct.phContext;

	/*
	 * Allocate the new hContext - if allocator full return an error
	 */
	rv = SCardAddContext(*phContext, dwClientID);

	return rv;
}

/**
 * @brief This function destroys a communication context to the PC/SC Resource
 * Manager. This must be the last function called in a PC/SC application.
 *
 * @ingroup API
 * @param[in] hContext Connection context to be closed.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hContext handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardReleaseContext(hContext);
 * @endcode
 */
LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	LONG rv;
	release_struct scReleaseStruct;
	sharedSegmentMsg msgStruct;
	LONG dwContextIndex;

	PROFILE_START

	/*
	 * Make sure this context has been opened
	 * and get dwContextIndex
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Remove the local context from the stack
		 */
		SCardLockThread();
		SCardRemoveContext(hContext);
		SCardUnlockThread();

		return rv;
	}

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the context is still opened */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		/* the context is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	scReleaseStruct.hContext = hContext;
	scReleaseStruct.rv = SCARD_S_SUCCESS;

	rv = WrapSHMWrite(SCARD_RELEASE_CONTEXT,
		psContextMap[dwContextIndex].dwClientID,
		sizeof(scReleaseStruct),
		PCSCLITE_MCLIENT_ATTEMPTS, (void *) &scReleaseStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);
	memcpy(&scReleaseStruct, &msgStruct.data, sizeof(scReleaseStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	/*
	 * Remove the local context from the stack
	 */
	SCardLockThread();
	SCardRemoveContext(hContext);
	SCardUnlockThread();

	PROFILE_END(scReleaseStruct.rv)

	return scReleaseStruct.rv;
}

/**
 * @brief The function does not do anything except returning \ref
 * SCARD_S_SUCCESS.
 *
 * @deprecated
 * This function is not in Microsoft(R) WinSCard API and is deprecated
 * in pcsc-lite API.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] dwTimeout New timeout value.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 */
LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout)
{
	/*
	 * Deprecated
	 */

	return SCARD_S_SUCCESS;
}

/**
 * @brief This function establishes a connection to the reader specified in \p
 * szReader.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] szReader Reader name to connect to.
 * @param[in] dwShareMode Mode of connection type: exclusive or shared.
 * - \ref SCARD_SHARE_SHARED - This application will allow others to share
 *   the reader.
 * - \ref SCARD_SHARE_EXCLUSIVE - This application will NOT allow others to
 *   share the reader.
 * - \ref SCARD_SHARE_DIRECT - Direct control of the reader, even without a
 *   card.  \ref SCARD_SHARE_DIRECT can be used before using SCardControl() to
 *   send control commands to the reader even if a card is not present in the
 *   reader.
 * @param[in] dwPreferredProtocols Desired protocol use.
 * - \ref SCARD_PROTOCOL_T0 - Use the T=0 protocol.
 * - \ref SCARD_PROTOCOL_T1 - Use the T=1 protocol.
 * - \ref SCARD_PROTOCOL_RAW - Use with memory type cards.
 * \p dwPreferredProtocols is a bit mask of acceptable protocols for the
 * connection. You can use (\ref SCARD_PROTOCOL_T0 | \ref SCARD_PROTOCOL_T1) if
 * you do not have a preferred protocol.
 * @param[out] phCard Handle to this connection.
 * @param[out] pdwActiveProtocol Established protocol to this connection.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hContext handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p phCard or \p pdwActiveProtocol is NULL (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid sharing mode, requested protocol, or reader name (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_READY Could not allocate the desired port (\ref SCARD_E_NOT_READY)
 * @retval SCARD_E_READER_UNAVAILABLE Could not power up the reader or card (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 * @retval SCARD_E_UNKNOWN_READER \p szReader is NULL (\ref SCARD_E_UNKNOWN_READER)
 * @retval SCARD_E_UNSUPPORTED_FEATURE Protocol not supported (\ref SCARD_E_UNSUPPORTED_FEATURE)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * @endcode
 */
LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	connect_struct scConnectStruct;
	sharedSegmentMsg msgStruct;
	LONG dwContextIndex;

	PROFILE_START

	/*
	 * Check for NULL parameters
	 */
	if (phCard == NULL || pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phCard = 0;

	if (szReader == NULL)
		return SCARD_E_UNKNOWN_READER;

	/*
	 * Check for uninitialized strings
	 */
	if (strlen(szReader) > MAX_READERNAME)
		return SCARD_E_INVALID_VALUE;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the context is still opened */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		/* the context is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	strncpy(scConnectStruct.szReader, szReader, MAX_READERNAME);

	scConnectStruct.hContext = hContext;
	scConnectStruct.dwShareMode = dwShareMode;
	scConnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scConnectStruct.phCard = 0;
	scConnectStruct.pdwActiveProtocol = 0;
	scConnectStruct.rv = SCARD_S_SUCCESS;

	rv = WrapSHMWrite(SCARD_CONNECT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scConnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scConnectStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scConnectStruct, &msgStruct.data, sizeof(scConnectStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	*phCard = scConnectStruct.phCard;
	*pdwActiveProtocol = scConnectStruct.pdwActiveProtocol;

	if (scConnectStruct.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally
		 */
		rv = SCardAddHandle(*phCard, dwContextIndex, szReader);
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

		PROFILE_END(rv)

		return rv;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scConnectStruct.rv)

	return scConnectStruct.rv;
}

/**
 * @brief This function reestablishes a connection to a reader that was
 * previously connected to using SCardConnect().
 *
 * In a multi application environment it is possible for an application to
 * reset the card in shared mode. When this occurs any other application trying
 * to access certain commands will be returned the value \ref
 * SCARD_W_RESET_CARD. When this occurs SCardReconnect() must be called in
 * order to acknowledge that the card was reset and allow it to change it's
 * state accordingly.
 *
 * @ingroup API
 * @param[in] hCard Handle to a previous call to connect.
 * @param[in] dwShareMode Mode of connection type: exclusive/shared.
 * - \ref SCARD_SHARE_SHARED - This application will allow others to share
 *   the reader.
 * - \ref SCARD_SHARE_EXCLUSIVE - This application will NOT allow others to
 *   share the reader.
 * @param[in] dwPreferredProtocols Desired protocol use.
 * - \ref SCARD_PROTOCOL_T0 - Use the T=0 protocol.
 * - \ref SCARD_PROTOCOL_T1 - Use the T=1 protocol.
 * - \ref SCARD_PROTOCOL_RAW - Use with memory type cards.
 * \p dwPreferredProtocols is a bit mask of acceptable protocols for
 * the connection. You can use (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1)
 * if you do not have a preferred protocol.
 * @param[in] dwInitialization Desired action taken on the card/reader.
 * - \ref SCARD_LEAVE_CARD - Do nothing.
 * - \ref SCARD_RESET_CARD - Reset the card (warm reset).
 * - \ref SCARD_UNPOWER_CARD - Unpower the card (cold reset).
 * - \ref SCARD_EJECT_CARD - Eject the card.
 * @param[out] pdwActiveProtocol Established protocol to this connection.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p phContext is null. (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid sharing mode, requested protocol, or reader name (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_READY Could not allocate the desired port (\ref SCARD_E_NOT_READY)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_UNSUPPORTED_FEATURE Protocol not supported (\ref SCARD_E_UNSUPPORTED_FEATURE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol, dwSendLength, dwRecvLength;
 * LONG rv;
 * BYTE pbRecvBuffer[10];
 * BYTE pbSendBuffer[] = {0xC0, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00};
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * ...
 * dwSendLength = sizeof(pbSendBuffer);
 * dwRecvLength = sizeof(pbRecvBuffer);
 * rv = SCardTransmit(hCard, SCARD_PCI_T0, pbSendBuffer, dwSendLength,
 *          &pioRecvPci, pbRecvBuffer, &dwRecvLength);
 * / * Card has been reset by another application * /
 * if (rv == SCARD_W_RESET_CARD)
 * {
 *   rv = SCardReconnect(hCard, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0,
 *            SCARD_RESET_CARD, &dwActiveProtocol);
 * }
 * @endcode
 */
LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	reconnect_struct scReconnectStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	if (pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	do
	{
		scReconnectStruct.hCard = hCard;
		scReconnectStruct.dwShareMode = dwShareMode;
		scReconnectStruct.dwPreferredProtocols = dwPreferredProtocols;
		scReconnectStruct.dwInitialization = dwInitialization;
		scReconnectStruct.pdwActiveProtocol = *pdwActiveProtocol;
		scReconnectStruct.rv = SCARD_S_SUCCESS;

		rv = WrapSHMWrite(SCARD_RECONNECT, psContextMap[dwContextIndex].dwClientID,
			sizeof(scReconnectStruct),
			PCSCLITE_CLIENT_ATTEMPTS, (void *) &scReconnectStruct);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS);

		memcpy(&scReconnectStruct, &msgStruct.data, sizeof(scReconnectStruct));

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}
	} while (SCARD_E_SHARING_VIOLATION == scReconnectStruct.rv);

	*pdwActiveProtocol = scReconnectStruct.pdwActiveProtocol;

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scReconnectStruct.rv)

	return scReconnectStruct.rv;
}

/**
 * @brief This function terminates a connection made through SCardConnect().
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwDisposition Reader function to execute.
 * - \ref SCARD_LEAVE_CARD - Do nothing.
 * - \ref SCARD_RESET_CARD - Reset the card (warm reset).
 * - \ref SCARD_UNPOWER_CARD - Unpower the card (cold reset).
 * - \ref SCARD_EJECT_CARD - Eject the card.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful(\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_VALUE Invalid \p dwDisposition (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
 * @endcode
 */
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	disconnect_struct scDisconnectStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	scDisconnectStruct.hCard = hCard;
	scDisconnectStruct.dwDisposition = dwDisposition;
	scDisconnectStruct.rv = SCARD_S_SUCCESS;

	rv = WrapSHMWrite(SCARD_DISCONNECT, psContextMap[dwContextIndex].dwClientID,
		sizeof(scDisconnectStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scDisconnectStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scDisconnectStruct, &msgStruct.data,
		sizeof(scDisconnectStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	SCardRemoveHandle(hCard);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scDisconnectStruct.rv)

	return scDisconnectStruct.rv;
}

/**
 * @brief This function establishes a temporary exclusive access mode for
 * doing a serie of commands in a transaction.
 *
 * You might want to use this when you are selecting a few files and then
 * writing a large file so you can make sure that another application will
 * not change the current file. If another application has a lock on this
 * reader or this application is in \ref SCARD_SHARE_EXCLUSIVE there will be no
 * action taken.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * rv = SCardBeginTransaction(hCard);
 * ...
 * / * Do some transmit commands * /
 * @endcode
 */
LONG SCardBeginTransaction(SCARDHANDLE hCard)
{

	LONG rv;
	begin_struct scBeginStruct;
	int i;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	scBeginStruct.hCard = hCard;
	scBeginStruct.rv = SCARD_S_SUCCESS;

	/*
	 * Query the server every so often until the sharing violation ends
	 * and then hold the lock for yourself.
	 */

	do
	{
		rv = WrapSHMWrite(SCARD_BEGIN_TRANSACTION, psContextMap[dwContextIndex].dwClientID,
			sizeof(scBeginStruct),
			PCSCLITE_CLIENT_ATTEMPTS, (void *) &scBeginStruct);

		if (rv == -1)
		{

			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS);

		memcpy(&scBeginStruct, &msgStruct.data, sizeof(scBeginStruct));

		if (rv == -1)
		{

			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}

	}
	while (scBeginStruct.rv == SCARD_E_SHARING_VIOLATION);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scBeginStruct.rv);

	return scBeginStruct.rv;
}

/**
 * @brief This function ends a previously begun transaction.
 *
 * The calling application must be the owner of the previously begun
 * transaction or an error will occur.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwDisposition Action to be taken on the reader.
 * The disposition action is not currently used in this release.
 * - \ref SCARD_LEAVE_CARD - Do nothing.
 * - \ref SCARD_RESET_CARD - Reset the card.
 * - \ref SCARD_UNPOWER_CARD - Unpower the card.
 * - \ref SCARD_EJECT_CARD - Eject the card.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_VALUE Invalid value for \p dwDisposition (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * rv = SCardBeginTransaction(hCard);
 * ...
 * / * Do some transmit commands * /
 * ...
 * rv = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
 * @endcode
 */
LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	end_struct scEndStruct;
	sharedSegmentMsg msgStruct;
	int randnum, i;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	/*
	 * Zero out everything
	 */
	randnum = 0;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	scEndStruct.hCard = hCard;
	scEndStruct.dwDisposition = dwDisposition;
	scEndStruct.rv = SCARD_S_SUCCESS;

	rv = WrapSHMWrite(SCARD_END_TRANSACTION,
		psContextMap[dwContextIndex].dwClientID,
		sizeof(scEndStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scEndStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scEndStruct, &msgStruct.data, sizeof(scEndStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	/*
	 * This helps prevent starvation
	 */
	randnum = SYS_RandomInt(1000, 10000);
	SYS_USleep(randnum);

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scEndStruct.rv)

	return scEndStruct.rv;
}

/**
 * @deprecated
 * This function is not in Microsoft(R) WinSCard API and is deprecated
 * in pcsc-lite API.
 * @ingroup API
 */
LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	cancel_struct scCancelStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	scCancelStruct.hCard = hCard;

	rv = WrapSHMWrite(SCARD_CANCEL_TRANSACTION,
		psContextMap[dwContextIndex].dwClientID,
		sizeof(scCancelStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scCancelStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scCancelStruct, &msgStruct.data, sizeof(scCancelStruct));

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(scCancelStruct.rv)

	return scCancelStruct.rv;
}

/**
 * @brief This function returns the current status of the reader connected to
 * by \p hCard.
 *
 * It's friendly name will be stored in \p szReaderName. \p pcchReaderLen will
 * be the size of the allocated buffer for \p szReaderName, while \p pcbAtrLen
 * will be the size of the allocated buffer for \p pbAtr. If either of these is
 * too small, the function will return with \ref SCARD_E_INSUFFICIENT_BUFFER
 * and the necessary size in \p pcchReaderLen and \p pcbAtrLen. The current
 * state, and protocol will be stored in pdwState and \p pdwProtocol
 * respectively.
 *
 * If \c *pcchReaderLen is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory for mszReaderName. Use
 * SCardFreeMemory() to release it.
 *
 * If \c *pcbAtrLen is equal to \ref SCARD_AUTOALLOCATE then the function will
 * allocate itself the needed memory for pbAtr. Use SCardFreeMemory() to
 * release it.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param mszReaderName [inout] Friendly name of this reader.
 * @param pcchReaderLen [inout] Size of the \p szReaderName multistring.
 * @param[out] pdwState Current state of this reader. \p pdwState
 * is a DWORD possibly OR'd with the following values:
 * - \ref SCARD_ABSENT - There is no card in the reader.
 * - \ref SCARD_PRESENT - There is a card in the reader, but it has not
 *   been moved into position for use.
 * - \ref SCARD_SWALLOWED - There is a card in the reader in position for
 *   use.  The card is not powered.
 * - \ref SCARD_POWERED - Power is being provided to the card, but the
 *   reader driver is unaware of the mode of the card.
 * - \ref SCARD_NEGOTIABLE - The card has been reset and is awaiting PTS
 *   negotiation.
 * - \ref SCARD_SPECIFIC - The card has been reset and specific
 *   communication protocols have been established.
 * @param[out] pdwProtocol Current protocol of this reader.
 * - \ref SCARD_PROTOCOL_T0 	Use the T=0 protocol.
 * - \ref SCARD_PROTOCOL_T1 	Use the T=1 protocol.
 * @param[out] pbAtr Current ATR of a card in this reader.
 * @param[out] pcbAtrLen Length of ATR.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Not enough allocated memory for \p szReaderName or for \p pbAtr (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pcchReaderLen or \p pcbAtrLen is NULL (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_W_REMOVED_CARD The smart card has been removed (\ref SCARD_W_REMOVED_CARD)
 * @retval SCARD_W_RESET_CARD The smart card has been reset (\ref SCARD_W_RESET_CARD)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * DWORD dwState, dwProtocol, dwAtrLen, dwReaderLen;
 * BYTE pbAtr[MAX_ATR_SIZE];
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * ...
 * dwAtrLen = sizeof(pbAtr);
 * rv = SCardStatus(hCard, NULL, &dwReaderLen, &dwState, &dwProtocol, pbAtr, &dwAtrLen);
 * @endcode
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * DWORD dwState, dwProtocol, dwAtrLen, dwReaderLen;
 * BYTE *pbAtr = NULL;
 * char *pcReader = NULL;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * ...
 * dwReaderLen = SCARD_AUTOALLOCATE;
 * dwAtrLen = SCARD_AUTOALLOCATE;
 * rv = SCardStatus(hCard, (LPSTR)&pcReader, &dwReaderLen, &dwState,
 *          &dwProtocol, (LPBYTE)&pbAtr, &dwAtrLen);
 * @endcode
 */
LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderName,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	DWORD dwReaderLen, dwAtrLen;
	LONG rv;
	int i;
	status_struct scStatusStruct;
	sharedSegmentMsg msgStruct;
	DWORD dwContextIndex, dwChannelIndex;
	char *r;
	char *bufReader = NULL;
	LPBYTE bufAtr = NULL;
	DWORD dummy;

	PROFILE_START

	/* default output values */
	if (pdwState)
		*pdwState = 0;

	if (pdwProtocol)
		*pdwProtocol = 0;

	/* Check for NULL parameters */ 
	if (pcchReaderLen == NULL)
		pcchReaderLen = &dummy;
	
	if (pcbAtrLen == NULL)
		pcbAtrLen = &dummy;

	/* length passed from caller */
	dwReaderLen = *pcchReaderLen;
	dwAtrLen = *pcbAtrLen;

	*pcchReaderLen = 0;
	*pcbAtrLen = 0;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		rv = SCARD_E_READER_UNAVAILABLE;
		goto end;
	}

	/* initialise the structure */
	memset(&scStatusStruct, 0, sizeof(scStatusStruct));
	scStatusStruct.hCard = hCard;

	/* those sizes need to be initialised */
	scStatusStruct.pcchReaderLen = sizeof(scStatusStruct.mszReaderNames);
	scStatusStruct.pcbAtrLen = sizeof(scStatusStruct.pbAtr);

	rv = WrapSHMWrite(SCARD_STATUS, psContextMap[dwContextIndex].dwClientID,
		sizeof(scStatusStruct),
		PCSCLITE_CLIENT_ATTEMPTS, (void *) &scStatusStruct);

	if (rv == -1)
	{
		rv = SCARD_E_NO_SERVICE;
		goto end;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	memcpy(&scStatusStruct, &msgStruct.data, sizeof(scStatusStruct));

	if (rv == -1)
	{
		rv = SCARD_F_COMM_ERROR;
		goto end;
	}

	rv = scStatusStruct.rv;
	if (rv != SCARD_S_SUCCESS && rv != SCARD_E_INSUFFICIENT_BUFFER)
	{
		/*
		 * An event must have occurred
		 */
		goto end;
	}

	/*
	 * Now continue with the client side SCardStatus
	 */

	*pcchReaderLen = strlen(psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName) + 1;
	*pcbAtrLen = (readerStates[i])->cardAtrLength;

	if (pdwState)
		*pdwState = (readerStates[i])->readerState;

	if (pdwProtocol)
		*pdwProtocol = (readerStates[i])->cardProtocol;

	if (SCARD_AUTOALLOCATE == dwReaderLen)
	{
		dwReaderLen = *pcchReaderLen;
		bufReader = malloc(dwReaderLen);
		if (NULL == bufReader)
		{
			rv = SCARD_E_NO_MEMORY;
			goto end;
		}
		if (NULL == mszReaderName)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		*(char **)mszReaderName = bufReader;
	}
	else
		bufReader = mszReaderName;

	/* return SCARD_E_INSUFFICIENT_BUFFER only if buffer pointer is non NULL */
	if (bufReader)
	{
		if (*pcchReaderLen > dwReaderLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;

		strncpy(bufReader,
			psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName,
			dwReaderLen);
	}

	if (SCARD_AUTOALLOCATE == dwAtrLen)
	{
		dwAtrLen = *pcbAtrLen;
		bufAtr = malloc(dwAtrLen);
		if (NULL == bufAtr)
		{
			rv = SCARD_E_NO_MEMORY;
			goto end;
		}
		if (NULL == pbAtr)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		*(LPBYTE *)pbAtr = bufAtr;
	}
	else
		bufAtr = pbAtr;

	if (bufAtr)
	{
		if (*pcbAtrLen > dwAtrLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;

		memcpy(bufAtr, (readerStates[i])->cardAtr, min(*pcbAtrLen, dwAtrLen));
	}

end:
	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(rv)

	return rv;
}

static long WaitForPcscdEvent(long dwTime)
{
	char filename[FILENAME_MAX];
	char buf[1];
	int fd;
	struct timeval tv, *ptv = NULL;
	struct timeval before, after;
	fd_set read_fd;

	if (INFINITE != dwTime)
	{
		if (dwTime < 0)
			return 0;
		gettimeofday(&before, NULL);
		tv.tv_sec = dwTime/1000;
		tv.tv_usec = dwTime*1000 - tv.tv_sec*1000000;
		ptv = &tv;
	}

	snprintf(filename, sizeof(filename), "%s/event.%d.%d", PCSCLITE_EVENTS_DIR,
		SYS_GetPID(), SYS_RandomInt(0, 0x10000));
	mkfifo(filename, 0644);
	fd = SYS_OpenFile(filename, O_RDONLY | O_NONBLOCK, 0);

	FD_ZERO(&read_fd);
	FD_SET(fd, &read_fd);
	
	select(fd+1, &read_fd, NULL, NULL, ptv);

	SYS_ReadFile(fd, buf, 1);
	SYS_CloseFile(fd);
	SYS_RemoveFile(filename);

	if (INFINITE != dwTime)
	{
		long int diff;

		gettimeofday(&after, NULL);
		diff = time_sub(&after, &before);
		dwTime -= diff/1000;
	}

	return dwTime;
}

/**
 * @brief This function receives a structure or list of structures containing
 * reader names. It then blocks for a change in state to occur on any of the
 * OR'd values contained in \p dwCurrentState for a maximum blocking time of
 * \p dwTimeout or forever if \ref INFINITE is used.
 *
 * The new event state will be contained in \p dwEventState. A status change
 * might be a card insertion or removal event, a change in ATR, etc.
 *
 * To wait for a reader event (reader added or removed) you may use the special
 * reader name \c "\\?PnP?\Notification". If a reader event occurs the state of
 * this reader will change and the bit \ref SCARD_STATE_CHANGED will be set.
 *
 * @code
 * typedef struct {
 *   LPCSTR szReader;          // Reader name
 *   LPVOID pvUserData;         // User defined data
 *   DWORD dwCurrentState;      // Current state of reader
 *   DWORD dwEventState;        // Reader state after a state change
 *   DWORD cbAtr;               // ATR Length, usually MAX_ATR_SIZE
 *   BYTE rgbAtr[MAX_ATR_SIZE]; // ATR Value
 * } SCARD_READERSTATE;
 * ...
 * typedef SCARD_READERSTATE *PSCARD_READERSTATE, **LPSCARD_READERSTATE;
 * ...
 * @endcode
 *
 * Value of \p dwCurrentState and \p dwEventState:
 * - \ref SCARD_STATE_UNAWARE The application is unaware of the current
 *   state, and would like to know. The use of this value results in an
 *   immediate return from state transition monitoring services. This is
 *   represented by all bits set to zero.
 * - \ref SCARD_STATE_IGNORE This reader should be ignored
 * - \ref SCARD_STATE_CHANGED There is a difference between the state
 *   believed by the application, and the state known by the resource
 *   manager.  When this bit is set, the application may assume a
 *   significant state change has occurred on this reader.
 * - \ref SCARD_STATE_UNKNOWN The given reader name is not recognized by the
 *   resource manager. If this bit is set, then \ref SCARD_STATE_CHANGED and
 *   \ref SCARD_STATE_IGNORE will also be set
 * - \ref SCARD_STATE_UNAVAILABLE The actual state of this reader is not
 *   available. If this bit is set, then all the following bits are clear.
 * - \ref SCARD_STATE_EMPTY There is no card in the reader. If this bit
 *   is set, all the following bits will be clear
 * - \ref SCARD_STATE_PRESENT There is a card in the reader
 * - \ref SCARD_STATE_ATRMATCH There is a card in the reader with an ATR
 *   matching one of the target cards. If this bit is set,
 *   \ref SCARD_STATE_PRESENT will also be set. This bit is only returned on
 *   the SCardLocateCards() function.
 * - \ref SCARD_STATE_EXCLUSIVE The card in the reader is allocated for
 *   exclusive use by another application. If this bit is set,
 *   \ref SCARD_STATE_PRESENT will also be set.
 * - \ref SCARD_STATE_INUSE The card in the reader is in use by one or more
 *   other applications, but may be connected to in shared mode. If this
 *   bit is set, \ref SCARD_STATE_PRESENT will also be set.
 * - \ref SCARD_STATE_MUTE There is an unresponsive card in the reader.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] dwTimeout Maximum waiting time (in miliseconds) for status
 *            change, zero (or \ref INFINITE) for infinite.
 * @param rgReaderStates [inout] Structures of readers with current states.
 * @param[in] cReaders Number of structures.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_SERVICE Server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_INVALID_PARAMETER \p rgReaderStates is NULL and \p cReaders > 0 (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid States, reader name, etc (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_INVALID_HANDLE Invalid hContext handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader is unavailable (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_TIMEOUT The user-specified timeout value has expired (\ref SCARD_E_TIMEOUT)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARD_READERSTATE_A rgReaderStates[2];
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * ...
 * rgReaderStates[0].szReader = "Reader X";
 * rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;
 *
 * rgReaderStates[1].szReader = "\\\\?PnP?\\Notification";
 * rgReaderStates[1].dwCurrentState = SCARD_STATE_UNAWARE;
 * ...
 * rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 2);
 * printf("reader state: 0x%04X\n", rgReaderStates[0].dwEventState);
 * printf("reader state: 0x%04X\n", rgReaderStates[1].dwEventState);
 * @endcode
 */
LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{
	PSCARD_READERSTATE_A currReader;
	PREADER_STATE rContext;
	long dwTime = dwTimeout;
	DWORD dwState;
	DWORD dwBreakFlag = 0;
	int j;
	LONG dwContextIndex;
	int currentReaderCount = 0;
	LONG rv = SCARD_S_SUCCESS;

	PROFILE_START

	if ((rgReaderStates == NULL && cReaders > 0)
		|| (cReaders > PCSCLITE_MAX_READERS_CONTEXTS))
		return SCARD_E_INVALID_PARAMETER;

	/* Check the integrity of the reader states structures */
	for (j = 0; j < cReaders; j++)
	{
		if (rgReaderStates[j].szReader == NULL)
			return SCARD_E_INVALID_VALUE;
	}

	/* return if all readers are SCARD_STATE_IGNORE */
	if (cReaders > 0)
	{
		int nbNonIgnoredReaders = cReaders;

		for (j=0; j<cReaders; j++)
			if (rgReaderStates[j].dwCurrentState & SCARD_STATE_IGNORE)
				nbNonIgnoredReaders--;

		if (0 == nbNonIgnoredReaders)
			return SCARD_S_SUCCESS;
	}

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the context is still opened */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		/* the context is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Application is waiting for a reader - return the first available
	 * reader
	 * This is DEPRECATED. Use the special reader name \\?PnP?\Notification
	 * instead
	 */
	if (cReaders == 0)
	{
		while (1)
		{
			int i;

			rv = SCardCheckDaemonAvailability();
			if (rv != SCARD_S_SUCCESS)
				goto end;

			for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
			{
				if ((readerStates[i])->readerID != 0)
				{
					/* Reader was found */
					rv = SCARD_S_SUCCESS;
					goto end;
				}
			}

			if (dwTimeout == 0)
			{
				/* return immediately - no reader available */
				rv = SCARD_E_READER_UNAVAILABLE;
				goto end;
			}

			dwTime = WaitForPcscdEvent(dwTime);
			if (dwTimeout != INFINITE)
			{
				if (dwTime <= 0)
				{
					rv = SCARD_E_TIMEOUT;
					goto end;
				}
			}
		}
	}

	/*
	 * End of search for readers
	 */

	/* Clear the event state for all readers */
	for (j = 0; j < cReaders; j++)
		rgReaderStates[j].dwEventState = 0;

	/* Now is where we start our event checking loop */
	Log1(PCSC_LOG_DEBUG, "Event Loop Start");

	psContextMap[dwContextIndex].contextBlockStatus = BLOCK_STATUS_BLOCKING;

	/* Get the initial reader count on the system */
	for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
		if ((readerStates[j])->readerID != 0)
			currentReaderCount++;

	j = 0;
	do
	{
		rv = SCardCheckDaemonAvailability();
		if (rv != SCARD_S_SUCCESS)
		{
			if (psContextMap[dwContextIndex].mMutex)
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

			PROFILE_END(rv)

			return rv;
		}

		currReader = &rgReaderStates[j];

		/* Ignore for IGNORED readers */
		if (!(currReader->dwCurrentState & SCARD_STATE_IGNORE))
		{
			LPSTR lpcReaderName;
			int i;

	  /************ Looks for correct readernames *********************/

			lpcReaderName = (char *) currReader->szReader;

			for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
			{
				if (strcmp(lpcReaderName, (readerStates[i])->readerName) == 0)
					break;
			}

			/* The requested reader name is not recognized */
			if (i == PCSCLITE_MAX_READERS_CONTEXTS)
			{
				/* PnP special reader? */
				if (strcasecmp(lpcReaderName, "\\\\?PnP?\\Notification") == 0)
				{
					int k, newReaderCount = 0;

					for (k=0; k < PCSCLITE_MAX_READERS_CONTEXTS; k++)
						if ((readerStates[k])->readerID != 0)
							newReaderCount++;

					if (newReaderCount != currentReaderCount)
					{
						Log1(PCSC_LOG_INFO, "Reader list changed");
						currentReaderCount = newReaderCount;

						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}
				else
				{
					currReader->dwEventState = SCARD_STATE_UNKNOWN | SCARD_STATE_UNAVAILABLE;
					if (!(currReader->dwCurrentState & SCARD_STATE_UNKNOWN))
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						/*
						 * Spec says use SCARD_STATE_IGNORE but a removed USB
						 * reader with eventState fed into currentState will
						 * be ignored forever
						 */
						dwBreakFlag = 1;
					}
				}
			}
			else
			{
				/* The reader has come back after being away */
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					dwBreakFlag = 1;
				}

	/*****************************************************************/

				/* Set the reader status structure */
				rContext = readerStates[i];

				/* Now we check all the Reader States */
				dwState = rContext->readerState;
				{
					int currentCounter, stateCounter;

					stateCounter = (dwState >> 16) & 0xFFFF;
					currentCounter = (currReader->dwCurrentState >> 16) & 0xFFFF;

					/* has the event counter changed since the last call? */
					if (stateCounter != currentCounter)
						currReader->dwEventState |= SCARD_STATE_CHANGED;

					/* add an event counter in the upper word of dwEventState */
					currReader->dwEventState =
						((currReader->dwEventState & 0xffff )
						| (stateCounter << 16));
				}

	/*********** Check if the reader is in the correct state ********/
				if (dwState & SCARD_UNKNOWN)
				{
					/* reader is in bad state */
					currReader->dwEventState = SCARD_STATE_UNAVAILABLE;
					if (!(currReader->dwCurrentState & SCARD_STATE_UNAVAILABLE))
					{
						/* App thinks reader is in good state and it is not */
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}
				else
				{
					/* App thinks reader in bad state but it is not */
					if (currReader-> dwCurrentState & SCARD_STATE_UNAVAILABLE)
					{
						currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

	/********** Check for card presence in the reader **************/

				if (dwState & SCARD_PRESENT)
				{
					/* card present but not yet powered up */
					if (0 == rContext->cardAtrLength)
						/* Allow the status thread to convey information */
						SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

					currReader->cbAtr = rContext->cardAtrLength;
					memcpy(currReader->rgbAtr, rContext->cardAtr,
						currReader->cbAtr);
				}
				else
					currReader->cbAtr = 0;

				/* Card is now absent */
				if (dwState & SCARD_ABSENT)
				{
					currReader->dwEventState |= SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_ATRMATCH;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;

					/* After present the rest are assumed */
					if (currReader->dwCurrentState & SCARD_STATE_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}
				/* Card is now present */
				else if (dwState & SCARD_PRESENT)
				{
					currReader->dwEventState |= SCARD_STATE_PRESENT;
					currReader->dwEventState &= ~SCARD_STATE_EMPTY;
					currReader->dwEventState &= ~SCARD_STATE_UNAWARE;
					currReader->dwEventState &= ~SCARD_STATE_IGNORE;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					currReader->dwEventState &= ~SCARD_STATE_UNAVAILABLE;
					currReader->dwEventState &= ~SCARD_STATE_MUTE;

					if (currReader->dwCurrentState & SCARD_STATE_EMPTY)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}

					if (dwState & SCARD_SWALLOWED)
					{
						currReader->dwEventState |= SCARD_STATE_MUTE;
						if (!(currReader->dwCurrentState & SCARD_STATE_MUTE))
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
					else
					{
						/* App thinks card is mute but it is not */
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}

				/* Now figure out sharing modes */
				if (rContext->readerSharing == -1)
				{
					currReader->dwEventState |= SCARD_STATE_EXCLUSIVE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}
				else if (rContext->readerSharing >= 1)
				{
					/* A card must be inserted for it to be INUSE */
					if (dwState & SCARD_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_INUSE;
						currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;
						if (currReader-> dwCurrentState & SCARD_STATE_EXCLUSIVE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							dwBreakFlag = 1;
						}
					}
				}
				else if (rContext->readerSharing == 0)
				{
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;

					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
					else if (currReader-> dwCurrentState
						& SCARD_STATE_EXCLUSIVE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						dwBreakFlag = 1;
					}
				}

				if (currReader->dwCurrentState == SCARD_STATE_UNAWARE)
				{
					/*
					 * Break out of the while .. loop and return status
					 * once all the status's for all readers is met
					 */
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					dwBreakFlag = 1;
				}
			}	/* End of SCARD_STATE_UNKNOWN */
		}	/* End of SCARD_STATE_IGNORE */

		/* Counter and resetter */
		j++;
		if (j == cReaders)
		{
			/* go back to the first reader */
			j = 0;

			/* Declare all the break conditions */

			/* Break if UNAWARE is set and all readers have been checked */
			if (dwBreakFlag == 1)
				break;

			if (BLOCK_STATUS_RESUME
				== psContextMap[dwContextIndex].contextBlockStatus)
				break;

			/* Only sleep once for each cycle of reader checks. */
			dwTime = WaitForPcscdEvent(dwTime);

			if (dwTimeout != INFINITE)
			{
				/* If time is greater than timeout and all readers have been
				 * checked
				 */
				if (dwTime <= 0)
				{
					rv = SCARD_E_TIMEOUT;
					goto end;
				}
			}
		}
	}
	while (1);

	if (psContextMap[dwContextIndex].contextBlockStatus == BLOCK_STATUS_RESUME)
		rv = SCARD_E_CANCELLED;

end:
	Log1(PCSC_LOG_DEBUG, "Event Loop End");

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief This function sends a command directly to the IFD Handler (reader
 * driver) to be processed by the reader.
 *
 * This is useful for creating client side reader drivers for functions like
 * PIN pads, biometrics, or other extensions to the normal smart card reader
 * that are not normally handled by PC/SC.
 *
 * @note the API of this function changed. In pcsc-lite 1.2.0 and before the
 * API was not Windows(R) PC/SC compatible. This has been corrected.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwControlCode Control code for the operation.\n
 * <a href="http://pcsclite.alioth.debian.org/pcsc-lite/node28.html">
 * Click here</a> for a list of supported commands by some drivers.
 * @param[in] pbSendBuffer Command to send to the reader.
 * @param[in] cbSendLength Length of the command.
 * @param[out] pbRecvBuffer Response from the reader.
 * @param[in] cbRecvLength Length of the response buffer.
 * @param[out] lpBytesReturned Length of the response.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_VALUE Invalid value was presented (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_INSUFFICIENT_BUFFER \p cbSendLength or \p cbRecvLength are too big (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed(\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_W_REMOVED_CARD The card has been removed from the reader(\ref SCARD_W_REMOVED_CARD)
 * @retval SCARD_W_RESET_CARD The card has been reset by another application (\ref SCARD_W_RESET_CARD)
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol, dwSendLength, dwRecvLength;
 * BYTE pbRecvBuffer[10];
 * BYTE pbSendBuffer[] = { 0x06, 0x00, 0x0A, 0x01, 0x01, 0x10 0x00 };
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_RAW, &hCard, &dwActiveProtocol);
 * dwSendLength = sizeof(pbSendBuffer);
 * dwRecvLength = sizeof(pbRecvBuffer);
 * rv = SCardControl(hCard, 0x42000001, pbSendBuffer, dwSendLength,
 *          pbRecvBuffer, sizeof(pbRecvBuffer), &dwRecvLength);
 * @endcode
 */
LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode, LPCVOID pbSendBuffer,
	DWORD cbSendLength, LPVOID pbRecvBuffer, DWORD cbRecvLength,
	LPDWORD lpBytesReturned)
{
	LONG rv;
	control_struct scControlStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	/* 0 bytes received by default */
	if (NULL != lpBytesReturned)
		*lpBytesReturned = 0;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE_EXTENDED)
		|| (cbRecvLength > MAX_BUFFER_SIZE_EXTENDED))
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE) || (cbRecvLength > MAX_BUFFER_SIZE))
	{
		/* extended control */
		unsigned char buffer[sizeof(sharedSegmentMsg) + MAX_BUFFER_SIZE_EXTENDED];
		control_struct_extended *scControlStructExtended = (control_struct_extended *)buffer;
		sharedSegmentMsg *pmsgStruct = (psharedSegmentMsg)buffer;

		scControlStructExtended->hCard = hCard;
		scControlStructExtended->dwControlCode = dwControlCode;
		scControlStructExtended->cbSendLength = cbSendLength;
		scControlStructExtended->cbRecvLength = cbRecvLength;
		scControlStructExtended->pdwBytesReturned = 0;
		scControlStructExtended->rv = SCARD_S_SUCCESS;
		/* The size of data to send is the size of
		 * struct control_struct_extended WITHOUT the data[] field
		 * plus the effective data[] size
		 */
		scControlStructExtended->size = sizeof(*scControlStructExtended)
			- (sizeof(control_struct_extended) - offsetof(control_struct_extended, data))
			+ cbSendLength;
		memcpy(scControlStructExtended->data, pbSendBuffer, cbSendLength);

		rv = WrapSHMWrite(SCARD_CONTROL_EXTENDED,
			psContextMap[dwContextIndex].dwClientID,
			scControlStructExtended->size,
			PCSCLITE_CLIENT_ATTEMPTS, buffer);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		/* read the first block */
		rv = SHMMessageReceive(buffer, sizeof(sharedSegmentMsg),
			psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);
		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}

		/* we receive a sharedSegmentMsg and not a control_struct_extended */
		scControlStructExtended = (control_struct_extended *)&(pmsgStruct -> data);

		/* a second block is present */
		if (scControlStructExtended->size > PCSCLITE_MAX_MESSAGE_SIZE)
		{
			rv = SHMMessageReceive(buffer + sizeof(sharedSegmentMsg),
				scControlStructExtended->size-PCSCLITE_MAX_MESSAGE_SIZE,
				psContextMap[dwContextIndex].dwClientID,
				PCSCLITE_CLIENT_ATTEMPTS);
			if (rv == -1)
			{
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
				return SCARD_F_COMM_ERROR;
			}
		}

		if (scControlStructExtended -> rv == SCARD_S_SUCCESS)
		{
			/*
			 * Copy and zero it so any secret information is not leaked
			 */
			memcpy(pbRecvBuffer, scControlStructExtended -> data,
				scControlStructExtended -> pdwBytesReturned);
			memset(scControlStructExtended -> data, 0x00,
				scControlStructExtended -> pdwBytesReturned);
		}

		if (NULL != lpBytesReturned)
			*lpBytesReturned = scControlStructExtended -> pdwBytesReturned;

		rv = scControlStructExtended -> rv;
	}
	else
	{
		scControlStruct.hCard = hCard;
		scControlStruct.dwControlCode = dwControlCode;
		scControlStruct.cbSendLength = cbSendLength;
		scControlStruct.cbRecvLength = cbRecvLength;
		memcpy(scControlStruct.pbSendBuffer, pbSendBuffer, cbSendLength);

		rv = WrapSHMWrite(SCARD_CONTROL,
			psContextMap[dwContextIndex].dwClientID,
			sizeof(scControlStruct), PCSCLITE_CLIENT_ATTEMPTS, &scControlStruct);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}

		memcpy(&scControlStruct, &msgStruct.data, sizeof(scControlStruct));

		if (NULL != lpBytesReturned)
			*lpBytesReturned = scControlStruct.dwBytesReturned;

		if (scControlStruct.rv == SCARD_S_SUCCESS)
		{
			/*
			 * Copy and zero it so any secret information is not leaked
			 */
			memcpy(pbRecvBuffer, scControlStruct.pbRecvBuffer,
				scControlStruct.cbRecvLength);
			memset(scControlStruct.pbRecvBuffer, 0x00,
				sizeof(scControlStruct.pbRecvBuffer));
		}

		rv = scControlStruct.rv;
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief This function get an attribute from the IFD Handler (reader driver).
 *
 * The list of possible attributes is available in the file \c reader.h.
 *
 * If \c *pcbAttrLen is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwAttrId Identifier for the attribute to get.
 * - \ref SCARD_ATTR_ASYNC_PROTOCOL_TYPES
 * - \ref SCARD_ATTR_ATR_STRING
 * - \ref SCARD_ATTR_CHANNEL_ID
 * - \ref SCARD_ATTR_CHARACTERISTICS
 * - \ref SCARD_ATTR_CURRENT_BWT
 * - \ref SCARD_ATTR_CURRENT_CLK
 * - \ref SCARD_ATTR_CURRENT_CWT
 * - \ref SCARD_ATTR_CURRENT_D
 * - \ref SCARD_ATTR_CURRENT_EBC_ENCODING
 * - \ref SCARD_ATTR_CURRENT_F
 * - \ref SCARD_ATTR_CURRENT_IFSC
 * - \ref SCARD_ATTR_CURRENT_IFSD
 * - \ref SCARD_ATTR_CURRENT_IO_STATE
 * - \ref SCARD_ATTR_CURRENT_N
 * - \ref SCARD_ATTR_CURRENT_PROTOCOL_TYPE
 * - \ref SCARD_ATTR_CURRENT_W
 * - \ref SCARD_ATTR_DEFAULT_CLK
 * - \ref SCARD_ATTR_DEFAULT_DATA_RATE
 * - \ref SCARD_ATTR_DEVICE_FRIENDLY_NAME
 * - \ref SCARD_ATTR_DEVICE_IN_USE
 * - \ref SCARD_ATTR_DEVICE_SYSTEM_NAME
 * - \ref SCARD_ATTR_DEVICE_UNIT
 * - \ref SCARD_ATTR_ESC_AUTHREQUEST
 * - \ref SCARD_ATTR_ESC_CANCEL
 * - \ref SCARD_ATTR_ESC_RESET
 * - \ref SCARD_ATTR_EXTENDED_BWT
 * - \ref SCARD_ATTR_ICC_INTERFACE_STATUS
 * - \ref SCARD_ATTR_ICC_PRESENCE
 * - \ref SCARD_ATTR_ICC_TYPE_PER_ATR
 * - \ref SCARD_ATTR_MAX_CLK
 * - \ref SCARD_ATTR_MAX_DATA_RATE
 * - \ref SCARD_ATTR_MAX_IFSD
 * - \ref SCARD_ATTR_MAXINPUT
 * - \ref SCARD_ATTR_POWER_MGMT_SUPPORT
 * - \ref SCARD_ATTR_SUPRESS_T1_IFS_REQUEST
 * - \ref SCARD_ATTR_SYNC_PROTOCOL_TYPES
 * - \ref SCARD_ATTR_USER_AUTH_INPUT_DEVICE
 * - \ref SCARD_ATTR_USER_TO_CARD_AUTH_DEVICE
 * - \ref SCARD_ATTR_VENDOR_IFD_SERIAL_NO
 * - \ref SCARD_ATTR_VENDOR_IFD_TYPE
 * - \ref SCARD_ATTR_VENDOR_IFD_VERSION
 * - \ref SCARD_ATTR_VENDOR_NAME
 *
 * Not all the \p dwAttrId values listed above may be implemented in the IFD
 * Handler you are using. And some \p dwAttrId values not listed here may be
 * implemented.
 *
 * @param[out] pbAttr Pointer to a buffer that receives the attribute.
 * @param pcbAttrLen [inout] Length of the \p pbAttr buffer in bytes.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Reader buffer not large enough (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * unsigned char pbAtr[MAX_ATR_SIZE];
 * DWORD dwAtrLen;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_RAW, &hCard, &dwActiveProtocol);
 * rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, pbAtr, &dwAtrLen);
 * @endcode
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * unsigned char *pbAttr;
 * DWORD dwAttrLen;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_RAW, &hCard, &dwActiveProtocol);
 * dwAttrLen = SCARD_AUTOALLOCATE;
 * rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, (unsigned char *)&pbAttr, &dwAttrLen);
 * @endcode
 */
LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPBYTE pbAttr,
	LPDWORD pcbAttrLen)
{
	LONG ret;
	unsigned char *buf = NULL;

	PROFILE_START

	if (NULL == pcbAttrLen)
		return SCARD_E_INVALID_PARAMETER;

	if (SCARD_AUTOALLOCATE == *pcbAttrLen)
	{
		if (NULL == pbAttr)
			return SCARD_E_INVALID_PARAMETER;

		*pcbAttrLen = MAX_BUFFER_SIZE;
		buf = malloc(*pcbAttrLen);
		if (NULL == buf)
			return SCARD_E_NO_MEMORY;

		*(unsigned char **)pbAttr = buf;
	}
	else
	{
		buf = pbAttr;

		/* if only get the length */
		if (NULL == pbAttr)
			/* use a reasonable size */
			*pcbAttrLen = MAX_BUFFER_SIZE;
	}

	ret = SCardGetSetAttrib(hCard, SCARD_GET_ATTRIB, dwAttrId, buf,
		pcbAttrLen);

	PROFILE_END(ret)

	return ret;
}

/**
 * @brief This function set an attribute of the IFD Handler.
 *
 * The list of attributes you can set is dependent on the IFD Handler you are
 * using.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwAttrId Identifier for the attribute to set.
 * @param[in] pbAttr Pointer to a buffer that receives the attribute.
 * @param[in] cbAttrLen Length of the \p pbAttr buffer in bytes.
 *
 * @return Error code
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * unsigned char pbAtr[MAX_ATR_SIZE];
 * DWORD dwAtrLen;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_RAW, &hCard, &dwActiveProtocol);
 * rv = SCardSetAttrib(hCard, 0x42000001, "\x12\x34\x56", 3);
 * @endcode
 */
LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId, LPCBYTE pbAttr,
	DWORD cbAttrLen)
{
	LONG ret;

	PROFILE_START

	if (NULL == pbAttr || 0 == cbAttrLen)
		return SCARD_E_INVALID_PARAMETER;

	ret = SCardGetSetAttrib(hCard, SCARD_SET_ATTRIB, dwAttrId, (LPBYTE)pbAttr,
		&cbAttrLen);

	PROFILE_END(ret)

	return ret;
}

static LONG SCardGetSetAttrib(SCARDHANDLE hCard, int command, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG rv;
	getset_struct scGetSetStruct;
	sharedSegmentMsg msgStruct;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	if (*pcbAttrLen > MAX_BUFFER_SIZE)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	scGetSetStruct.hCard = hCard;
	scGetSetStruct.dwAttrId = dwAttrId;
	scGetSetStruct.cbAttrLen = *pcbAttrLen;
	scGetSetStruct.rv = SCARD_E_NO_SERVICE;
	memset(scGetSetStruct.pbAttr, 0, sizeof(scGetSetStruct.pbAttr));
	if (SCARD_SET_ATTRIB == command)
		memcpy(scGetSetStruct.pbAttr, pbAttr, *pcbAttrLen);

	rv = WrapSHMWrite(command,
		psContextMap[dwContextIndex].dwClientID, sizeof(scGetSetStruct),
		PCSCLITE_CLIENT_ATTEMPTS, &scGetSetStruct);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_NO_SERVICE;
	}

	/*
	 * Read a message from the server
	 */
	rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
		PCSCLITE_CLIENT_ATTEMPTS);

	if (rv == -1)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_F_COMM_ERROR;
	}

	memcpy(&scGetSetStruct, &msgStruct.data, sizeof(scGetSetStruct));

	if ((SCARD_S_SUCCESS == scGetSetStruct.rv) && (SCARD_GET_ATTRIB == command))
	{
		/*
		 * Copy and zero it so any secret information is not leaked
		 */
		if (*pcbAttrLen < scGetSetStruct.cbAttrLen)
		{
			scGetSetStruct.cbAttrLen = *pcbAttrLen;
			scGetSetStruct.rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
		else
			*pcbAttrLen = scGetSetStruct.cbAttrLen;

		if (pbAttr)
			memcpy(pbAttr, scGetSetStruct.pbAttr, scGetSetStruct.cbAttrLen);

		memset(scGetSetStruct.pbAttr, 0x00, sizeof(scGetSetStruct.pbAttr));
	}

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	return scGetSetStruct.rv;
}

/**
 * @brief This function sends an APDU to the smart card contained in the reader
 * connected to by SCardConnect().
 *
 * The card responds from the APDU and stores this response in \p pbRecvBuffer
 * and it's length in \p pcbRecvLength.
 * \p pioSendPci and \p pioRecvPci are structures containing the following:
 * @code
 * typedef struct {
 *    DWORD dwProtocol;    // SCARD_PROTOCOL_T0 or SCARD_PROTOCOL_T1
 *    DWORD cbPciLength;   // Length of this structure - not used
 * } SCARD_IO_REQUEST;
 * @endcode
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param pioSendPci [inout] Structure of Protocol Control Information.
 * - \ref SCARD_PCI_T0 - Pre-defined T=0 PCI structure.
 * - \ref SCARD_PCI_T1 - Pre-defined T=1 PCI structure.
 * - \ref SCARD_PCI_RAW - Pre-defined RAW PCI structure.
 * @param[in] pbSendBuffer APDU to send to the card.
 * @param[in] cbSendLength Length of the APDU.
 * @param pioRecvPci [inout] Structure of protocol information.
 * @param[out] pbRecvBuffer Response from the card.
 * @param pcbRecvLength [inout] Length of the response.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER \p cbSendLength or \p cbRecvLength are too big (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pbSendBuffer or \p pbRecvBuffer or \p pcbRecvLength or \p pioSendPci is null (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid Protocol, reader name, etc (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED APDU exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_PROTO_MISMATCH Connect protocol is different than desired (\ref SCARD_E_PROTO_MISMATCH)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_W_RESET_CARD The card has been reset by another application (\ref SCARD_W_RESET_CARD)
 * @retval SCARD_W_REMOVED_CARD The card has been removed from the reader (\ref SCARD_W_REMOVED_CARD)
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol, dwSendLength, dwRecvLength;
 * SCARD_IO_REQUEST pioRecvPci;
 * BYTE pbRecvBuffer[10];
 * BYTE pbSendBuffer[] = { 0xC0, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * dwSendLength = sizeof(pbSendBuffer);
 * dwRecvLength = sizeof(pbRecvBuffer);
 * rv = SCardTransmit(hCard, SCARD_PCI_T0, pbSendBuffer, dwSendLength,
 *          &pioRecvPci, pbRecvBuffer, &dwRecvLength);
 * @endcode
 */
LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	int i;
	DWORD dwContextIndex, dwChannelIndex;

	PROFILE_START

	if (pbSendBuffer == NULL || pbRecvBuffer == NULL ||
			pcbRecvLength == NULL || pioSendPci == NULL)
		return SCARD_E_INVALID_PARAMETER;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INVALID_HANDLE;
	}

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the handle is still valid */
	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndex, &dwChannelIndex);
	if (rv == -1)
		/* the handle is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		char *r = psContextMap[dwContextIndex].psChannelMap[dwChannelIndex].readerName;

		/* by default r == NULL */
		if (r && strcmp(r, (readerStates[i])->readerName) == 0)
			break;
	}

	if (i == PCSCLITE_MAX_READERS_CONTEXTS)
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_READER_UNAVAILABLE;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE_EXTENDED)
		|| (*pcbRecvLength > MAX_BUFFER_SIZE_EXTENDED))
	{
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE) || (*pcbRecvLength > MAX_BUFFER_SIZE))
	{
		/* extended APDU */
		unsigned char buffer[sizeof(sharedSegmentMsg) + MAX_BUFFER_SIZE_EXTENDED];
		transmit_struct_extended *scTransmitStructExtended = (transmit_struct_extended *)buffer;
		sharedSegmentMsg *pmsgStruct = (psharedSegmentMsg)buffer;

		scTransmitStructExtended->hCard = hCard;
		scTransmitStructExtended->cbSendLength = cbSendLength;
		scTransmitStructExtended->pcbRecvLength = *pcbRecvLength;
		/* The size of data to send is the size of
		 * struct control_struct_extended WITHOUT the data[] field
		 * plus the effective data[] size
		 */
		scTransmitStructExtended->size = sizeof(*scTransmitStructExtended)
			- (sizeof(transmit_struct_extended) - offsetof(transmit_struct_extended, data))
			+ cbSendLength;
		scTransmitStructExtended->pioSendPciProtocol = pioSendPci->dwProtocol;
		scTransmitStructExtended->pioSendPciLength = pioSendPci->cbPciLength;
		memcpy(scTransmitStructExtended->data, pbSendBuffer, cbSendLength);
		scTransmitStructExtended->rv = SCARD_S_SUCCESS;

		if (pioRecvPci)
		{
			scTransmitStructExtended->pioRecvPciProtocol = pioRecvPci->dwProtocol;
			scTransmitStructExtended->pioRecvPciLength = pioRecvPci->cbPciLength;
		}
		else
		{
			scTransmitStructExtended->pioRecvPciProtocol = SCARD_PROTOCOL_ANY;
			scTransmitStructExtended->pioRecvPciLength = sizeof(SCARD_IO_REQUEST);
		}

		rv = WrapSHMWrite(SCARD_TRANSMIT_EXTENDED,
			psContextMap[dwContextIndex].dwClientID,
			scTransmitStructExtended->size,
			PCSCLITE_CLIENT_ATTEMPTS, buffer);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		/* read the first block */
		rv = SHMMessageReceive(buffer, sizeof(sharedSegmentMsg), psContextMap[dwContextIndex].dwClientID, PCSCLITE_CLIENT_ATTEMPTS);
		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}

		/* we receive a sharedSegmentMsg and not a transmit_struct_extended */
		scTransmitStructExtended = (transmit_struct_extended *)&(pmsgStruct -> data);

		/* a second block is present */
		if (scTransmitStructExtended->size > PCSCLITE_MAX_MESSAGE_SIZE)
		{
			rv = SHMMessageReceive(buffer + sizeof(sharedSegmentMsg),
				scTransmitStructExtended->size-PCSCLITE_MAX_MESSAGE_SIZE,
				psContextMap[dwContextIndex].dwClientID,
				PCSCLITE_CLIENT_ATTEMPTS);
			if (rv == -1)
			{
				SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
				return SCARD_F_COMM_ERROR;
			}
		}

		if (scTransmitStructExtended -> rv == SCARD_S_SUCCESS)
		{
			/*
			 * Copy and zero it so any secret information is not leaked
			 */
			memcpy(pbRecvBuffer, scTransmitStructExtended -> data,
				scTransmitStructExtended -> pcbRecvLength);
			memset(scTransmitStructExtended -> data, 0x00,
				scTransmitStructExtended -> pcbRecvLength);

			if (pioRecvPci)
			{
				pioRecvPci->dwProtocol = scTransmitStructExtended->pioRecvPciProtocol;
				pioRecvPci->cbPciLength = scTransmitStructExtended->pioRecvPciLength;
			}
		}

		*pcbRecvLength = scTransmitStructExtended -> pcbRecvLength;
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

		rv = scTransmitStructExtended -> rv;
	}
	else
	{
		/* short APDU */
		transmit_struct scTransmitStruct;
		sharedSegmentMsg msgStruct;

		scTransmitStruct.hCard = hCard;
		scTransmitStruct.cbSendLength = cbSendLength;
		scTransmitStruct.pcbRecvLength = *pcbRecvLength;
		scTransmitStruct.pioSendPciProtocol = pioSendPci->dwProtocol;
		scTransmitStruct.pioSendPciLength = pioSendPci->cbPciLength;
		memcpy(scTransmitStruct.pbSendBuffer, pbSendBuffer, cbSendLength);
		memset(scTransmitStruct.pbSendBuffer+cbSendLength, 0, sizeof(scTransmitStruct.pbSendBuffer)-cbSendLength);
		memset(scTransmitStruct.pbRecvBuffer, 0, sizeof(scTransmitStruct.pbRecvBuffer));
		scTransmitStruct.rv = SCARD_S_SUCCESS;

		if (pioRecvPci)
		{
			scTransmitStruct.pioRecvPciProtocol = pioRecvPci->dwProtocol;
			scTransmitStruct.pioRecvPciLength = pioRecvPci->cbPciLength;
		}
		else
		{
			scTransmitStruct.pioRecvPciProtocol = SCARD_PROTOCOL_ANY;
			scTransmitStruct.pioRecvPciLength = sizeof(SCARD_IO_REQUEST);
		}

		rv = WrapSHMWrite(SCARD_TRANSMIT,
			psContextMap[dwContextIndex].dwClientID, sizeof(scTransmitStruct),
			PCSCLITE_CLIENT_ATTEMPTS, (void *) &scTransmitStruct);

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_E_NO_SERVICE;
		}

		/*
		 * Read a message from the server
		 */
		rv = SHMClientRead(&msgStruct, psContextMap[dwContextIndex].dwClientID,
			PCSCLITE_CLIENT_ATTEMPTS);

		memcpy(&scTransmitStruct, &msgStruct.data, sizeof(scTransmitStruct));

		if (rv == -1)
		{
			SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);
			return SCARD_F_COMM_ERROR;
		}

		/*
		 * Zero it and free it so any secret information cannot be leaked
		 */
		memset(scTransmitStruct.pbSendBuffer, 0x00, cbSendLength);

		if (scTransmitStruct.rv == SCARD_S_SUCCESS)
		{
			/*
			 * Copy and zero it so any secret information is not leaked
			 */
			memcpy(pbRecvBuffer, scTransmitStruct.pbRecvBuffer,
				scTransmitStruct.pcbRecvLength);
			memset(scTransmitStruct.pbRecvBuffer, 0x00,
				scTransmitStruct.pcbRecvLength);

			if (pioRecvPci)
			{
				pioRecvPci->dwProtocol = scTransmitStruct.pioRecvPciProtocol;
				pioRecvPci->cbPciLength = scTransmitStruct.pioRecvPciLength;
			}
		}

		*pcbRecvLength = scTransmitStruct.pcbRecvLength;
		SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

		rv = scTransmitStruct.rv;
	}

	PROFILE_END(rv)

	return rv;
}

/**
 * This function returns a list of currently available readers on the system.
 *
 * \p mszReaders is a pointer to a character string that is allocated by the
 * application.  If the application sends \p mszGroups and \p mszReaders as
 * NULL then this function will return the size of the buffer needed to
 * allocate in \p pcchReaders.
 *
 * If \c *pcchReaders is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] mszGroups List of groups to list readers (not used).
 * @param[out] mszReaders Multi-string with list of readers.
 * @param pcchReaders [inout] Size of multi-string buffer including NULL's.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid Scope Handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pcchReaders is NULL (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Reader buffer not large enough (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszReaders;
 * DWORD dwReaders;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
 * mszReaders = malloc(sizeof(char)*dwReaders);
 * rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
 * @endcode
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszReaders;
 * DWORD dwReaders;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * dwReaders = SCARD_AUTOALLOCATE
 * rv = SCardListReaders(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
 * rv = SCardFreeMemory(hContext, mszReaders);
 * @endcode
 */
LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	DWORD dwReadersLen;
	int i;
	LONG dwContextIndex;
	LONG rv = SCARD_S_SUCCESS;
	char *buf = NULL;

	PROFILE_START

	/*
	 * Check for NULL parameters
	 */
	if (pcchReaders == NULL)
		return SCARD_E_INVALID_PARAMETER;

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the context is still opened */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		/* the context is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	dwReadersLen = 0;
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		if ((readerStates[i])->readerID != 0)
			dwReadersLen += strlen((readerStates[i])->readerName) + 1;

	/* for the last NULL byte */
	dwReadersLen += 1;

	if (1 == dwReadersLen)
	{
		rv = SCARD_E_NO_READERS_AVAILABLE;
		goto end;
	}

	if (SCARD_AUTOALLOCATE == *pcchReaders)
	{
		buf = malloc(dwReadersLen);
		if (NULL == buf)
		{
			rv = SCARD_E_NO_MEMORY;
			goto end;
		}
		if (NULL == mszReaders)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		*(char **)mszReaders = buf;
	}
	else
	{
		buf = mszReaders;

		/* not enough place to store the reader names */
		if ((NULL != mszReaders) && (*pcchReaders < dwReadersLen))
		{
			rv = SCARD_E_INSUFFICIENT_BUFFER;
			goto end;
		}
	}

	if (mszReaders == NULL)	/* text array not allocated */
		goto end;

	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		if ((readerStates[i])->readerID != 0)
		{
			/*
			 * Build the multi-string
			 */
			strcpy(buf, (readerStates[i])->readerName);
			buf += strlen((readerStates[i])->readerName)+1;
		}
	}
	*buf = '\0';	/* Add the last null */

end:
	/* set the reader names length */
	*pcchReaders = dwReadersLen;

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief Releases memory that has been returned from the resource manager
 * using the \ref SCARD_AUTOALLOCATE length designator.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] pvMem pointer to allocated memory
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 */

LONG SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG rv = SCARD_S_SUCCESS;
	LONG dwContextIndex;

	PROFILE_START

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	free((void *)pvMem);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief This function returns a list of currently available reader groups on
 * the system. \p mszGroups is a pointer to a character string that is
 * allocated by the application.  If the application sends \p mszGroups as NULL
 * then this function will return the size of the buffer needed to allocate in
 * \p pcchGroups.
 *
 * The group names is a multi-string and separated by a nul character (\c
 * '\\0') and ended by a double nul character like
 * \c "SCard$DefaultReaders\\0Group 2\\0\\0".
 *
 * If \c *pcchGroups is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[out] mszGroups List of groups to list readers.
 * @param pcchGroups [inout] Size of multi-string buffer including NUL's.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Reader buffer not large enough (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid Scope Handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_NO_SERVICE The server is not runing (\ref SCARD_E_NO_SERVICE)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszGroups;
 * DWORD dwGroups;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardListReaderGroups(hContext, NULL, &dwGroups);
 * mszGroups = malloc(sizeof(char)*dwGroups);
 * rv = SCardListReaderGroups(hContext, mszGroups, &dwGroups);
 * @endcode
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszGroups;
 * DWORD dwGroups;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * dwGroups = SCARD_AUTOALLOCATE;
 * rv = SCardListReaderGroups(hContext, (LPSTR)&mszGroups, &dwGroups);
 * rv = SCardFreeMemory(hContext, mszGroups);
 * @endcode
 */
LONG SCardListReaderGroups(SCARDCONTEXT hContext, LPSTR mszGroups,
	LPDWORD pcchGroups)
{
	LONG rv = SCARD_S_SUCCESS;
	LONG dwContextIndex;
	char *buf = NULL;

	PROFILE_START

	/* Multi-string with two trailing \0 */
	const char ReaderGroup[] = "SCard$DefaultReaders\0";
	const int dwGroups = sizeof(ReaderGroup);

	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	SYS_MutexLock(psContextMap[dwContextIndex].mMutex);

	/* check the context is still opened */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		/* the context is now invalid
		 * -> another thread may have called SCardReleaseContext
		 * -> so the mMutex has been unlocked */
		return SCARD_E_INVALID_HANDLE;

	if (SCARD_AUTOALLOCATE == *pcchGroups)
	{
		buf = malloc(dwGroups);
		if (NULL == buf)
		{
			rv = SCARD_E_NO_MEMORY;
			goto end;
		}
		if (NULL == mszGroups)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		*(char **)mszGroups = buf;
	}
	else
	{
		buf = mszGroups;

		if ((NULL != mszGroups) && (*pcchGroups < dwGroups))
		{
			rv = SCARD_E_INSUFFICIENT_BUFFER;
			goto end;
		}
	}

	if (buf)
		memcpy(buf, ReaderGroup, dwGroups);

end:
	*pcchGroups = dwGroups;

	SYS_MutexUnLock(psContextMap[dwContextIndex].mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * This function cancels all pending blocking requests on the
 * SCardGetStatusChange() function.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hContext handle (\ref SCARD_E_INVALID_HANDLE)
 *
 * @code
 * SCARDCONTEXT hContext;
 * DWORD cReaders;
 * SCARD_READERSTATE rgReaderStates;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rgReaderStates.szReader = strdup("Reader X");
 * rgReaderStates.dwCurrentState = SCARD_STATE_EMPTY;
 * ...
 * / * Spawn off thread for following function * /
 * ...
 * rv = SCardGetStatusChange(hContext, 0, rgReaderStates, cReaders);
 * rv = SCardCancel(hContext);
 * @endcode
 */
LONG SCardCancel(SCARDCONTEXT hContext)
{
	LONG dwContextIndex;

	PROFILE_START

	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Set the block status for this Context so blocking calls will
	 * complete
	 */
	psContextMap[dwContextIndex].contextBlockStatus = BLOCK_STATUS_RESUME;

	PROFILE_END(SCARD_S_SUCCESS)

	return SCARD_S_SUCCESS;
}

/**
 * @brief Check if a \ref SCARDCONTEXT is valid.
 *
 * Call this function to determine whether a smart card context handle is still
 * valid. After a smart card context handle has been set by
 * SCardEstablishContext(), it may become not valid if the resource manager
 * service has been shut down.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid Handle (\ref SCARD_E_INVALID_HANDLE)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardIsValidContext(hContext);
 * @endcode
 */
LONG SCardIsValidContext(SCARDCONTEXT hContext)
{
	LONG rv;
	LONG dwContextIndex;

	PROFILE_START

	rv = SCARD_S_SUCCESS;

	/* Check if the _same_ server is running */
	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure this context has been opened
	 */
	dwContextIndex = SCardGetContextIndice(hContext);
	if (dwContextIndex == -1)
		rv = SCARD_E_INVALID_HANDLE;

	PROFILE_END(rv)

	return rv;
}

/**
 * Functions for managing instances of SCardEstablishContext() These functions
 * keep track of Context handles and associate the blocking
 * variable contextBlockStatus to an hContext
 */

/**
 * @brief Adds an Application Context to the vector \c _psContextMap.
 *
 * @param[in] hContext Application Context ID.
 * @param[in] dwClientID Client connection ID.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Success (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_MEMORY There is no free slot to store \p hContext (\ref SCARD_E_NO_MEMORY)
 */
static LONG SCardAddContext(SCARDCONTEXT hContext, DWORD dwClientID)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext == 0)
		{
			psContextMap[i].hContext = hContext;
			psContextMap[i].dwClientID = dwClientID;
			psContextMap[i].contextBlockStatus = BLOCK_STATUS_RESUME;
			psContextMap[i].mMutex = malloc(sizeof(PCSCLITE_MUTEX));
			SYS_MutexInit(psContextMap[i].mMutex);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

/**
 * @brief Get the index from the Application Context vector \c _psContextMap
 * for the passed context.
 *
 * This function is a thread-safe wrapper to the function
 * SCardGetContextIndiceTH().
 *
 * @param[in] hContext Application Context whose index will be find.
 *
 * @return Index corresponding to the Application Context or -1 if it is
 * not found.
 */
static LONG SCardGetContextIndice(SCARDCONTEXT hContext)
{
	LONG rv;

	SCardLockThread();
	rv = SCardGetContextIndiceTH(hContext);
	SCardUnlockThread();

	return rv;
}

/**
 * @brief Get the index from the Application Context vector \c _psContextMap
 * for the passed context.
 *
 * This functions is not thread-safe and should not be called. Instead, call
 * the function SCardGetContextIndice().
 *
 * @param[in] hContext Application Context whose index will be find.
 *
 * @return Index corresponding to the Application Context or -1 if it is
 * not found.
 */
static LONG SCardGetContextIndiceTH(SCARDCONTEXT hContext)
{
	int i;

	/*
	 * Find this context and return its spot in the array
	 */
	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if ((hContext == psContextMap[i].hContext) && (hContext != 0))
			return i;
	}

	return -1;
}

/**
 * @brief Removes an Application Context from a control vector.
 *
 * @param[in] hContext Application Context to be removed.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Success (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE The context \p hContext was not found (\ref SCARD_E_INVALID_HANDLE)
 */
static LONG SCardRemoveContext(SCARDCONTEXT hContext)
{
	LONG  retIndice;

	retIndice = SCardGetContextIndiceTH(hContext);

	if (retIndice == -1)
		return SCARD_E_INVALID_HANDLE;
	else
		return SCardCleanContext(retIndice);
}

static LONG SCardCleanContext(LONG indice)
{
	int i;

	psContextMap[indice].hContext = 0;
	SHMClientCloseSession(psContextMap[indice].dwClientID);
	psContextMap[indice].dwClientID = 0;
	free(psContextMap[indice].mMutex);
	psContextMap[indice].mMutex = NULL;
	psContextMap[indice].contextBlockStatus = BLOCK_STATUS_RESUME;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		/*
		 * Reset the \c hCard structs to zero
		 */
		psContextMap[indice].psChannelMap[i].hCard = 0;
		free(psContextMap[indice].psChannelMap[i].readerName);
		psContextMap[indice].psChannelMap[i].readerName = NULL;
	}

	return SCARD_S_SUCCESS;
}

/*
 * Functions for managing hCard values returned from SCardConnect.
 */

static LONG SCardAddHandle(SCARDHANDLE hCard, DWORD dwContextIndex,
	LPCSTR readerName)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (psContextMap[dwContextIndex].psChannelMap[i].hCard == 0)
		{
			psContextMap[dwContextIndex].psChannelMap[i].hCard = hCard;
			psContextMap[dwContextIndex].psChannelMap[i].readerName = strdup(readerName);
			return SCARD_S_SUCCESS;
		}
	}

	return SCARD_E_NO_MEMORY;
}

static LONG SCardRemoveHandle(SCARDHANDLE hCard)
{
	DWORD dwContextIndice, dwChannelIndice;
	LONG rv;

	rv = SCardGetIndicesFromHandle(hCard, &dwContextIndice, &dwChannelIndice);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;
	else
	{
		psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].hCard = 0;
		free(psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].readerName);
		psContextMap[dwContextIndice].psChannelMap[dwChannelIndice].readerName = NULL;
		return SCARD_S_SUCCESS;
	}
}

static LONG SCardGetIndicesFromHandle(SCARDHANDLE hCard,
	PDWORD pdwContextIndice, PDWORD pdwChannelIndice)
{
	LONG rv;

	if (0 == hCard)
		return -1;

	SCardLockThread();
	rv = SCardGetIndicesFromHandleTH(hCard, pdwContextIndice, pdwChannelIndice);
	SCardUnlockThread();

	return rv;
}

static LONG SCardGetIndicesFromHandleTH(SCARDHANDLE hCard,
	PDWORD pdwContextIndice, PDWORD pdwChannelIndice)
{
	int i;

	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
	{
		if (psContextMap[i].hContext != 0)
		{
			int j;

			for (j = 0; j < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; j++)
			{
				if (psContextMap[i].psChannelMap[j].hCard == hCard)
				{
					*pdwContextIndice = i;
					*pdwChannelIndice = j;
					return SCARD_S_SUCCESS;
				}
			}

		}
	}

	return -1;
}

/**
 * @brief Checks if the server is running.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Server is running (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_SERVICE Server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_INVALID_HANDLE Server was restarted or after fork() (\ref SCARD_E_INVALID_HANDLE)
 */
LONG SCardCheckDaemonAvailability(void)
{
	LONG rv;
	struct stat statBuffer;
	int need_restart = 0;

	rv = SYS_Stat(PCSCLITE_PUBSHM_FILE, &statBuffer);

	if (rv != 0)
	{
		Log2(PCSC_LOG_INFO, "PCSC Not Running: " PCSCLITE_PUBSHM_FILE ": %s",
			strerror(errno));
		return SCARD_E_NO_SERVICE;
	}

	/* when the _first_ reader is connected the ctime changes
	 * I don't know why yet */
	if (daemon_ctime && statBuffer.st_ctime > daemon_ctime)
	{
		/* so we also check the daemon pid to be sure it is a new pcscd */
		if (GetDaemonPid() != daemon_pid)
		{
			Log1(PCSC_LOG_INFO, "PCSC restarted");
			need_restart = 1;
		}
	}

	/* after fork() need to restart */
	if (client_pid && client_pid != getpid())
	{
		Log1(PCSC_LOG_INFO, "Client forked");
		need_restart = 1;
	}

	if (need_restart)
	{
		int i;

		/* invalid all handles */
		SCardLockThread();

		for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXTS; i++)
			if (psContextMap[i].hContext)
				SCardCleanContext(i);

		SCardUnlockThread();

		/* reset pcscd status */
		daemon_ctime = 0;
		client_pid = 0;

		/* reset the lib */
		SCardUnload();

		return SCARD_E_INVALID_HANDLE;
	}

	daemon_ctime = statBuffer.st_ctime;
	daemon_pid = GetDaemonPid();
	client_pid = getpid();

	return SCARD_S_SUCCESS;
}

/**
 * @brief Free resources allocated by the library
 *
 * You _shall_ call this function if you use dlopen/dlclose to load/unload the
 * library. Otherwise you will exhaust the ressources available.
 */
#ifdef __SUNPRO_C
#pragma fini (SCardUnload)
#endif

void DESTRUCTOR SCardUnload(void)
{
	int i;

	if (!isExecuted)
		return;

	/* unmap public shared file from memory */
	for (i = 0; i < PCSCLITE_MAX_APPLICATION_CONTEXT_CHANNELS; i++)
	{
		if (readerStates[i] != NULL)
		{
			SYS_PublicMemoryUnmap(readerStates[i], sizeof(READER_STATE));
			readerStates[i] = NULL;
		}
	}

	SYS_CloseFile(mapAddr);
	isExecuted = 0;
}

