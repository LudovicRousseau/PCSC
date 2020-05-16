/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2005
 *  Martin Paljak <martin@paljak.pri.ee>
 * Copyright (C) 2002-2011
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 * Copyright (C) 2009
 *  Jean-Luc Giraud <jlgiraud@googlemail.com>
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
 * @defgroup API API
 * @brief Handles smart card reader communications and
 * forwarding requests over message queues.
 *
 * Here is exposed the API for client applications.
 *
 * \anchor differences
 * @attention
 * Known \ref differences with Microsoft Windows WinSCard implementation:
 *
 * -# SCardStatus()
 *    @par
 *    SCardStatus() returns a bit field on pcsc-lite but a enumeration on
 *    Windows.
 *    @par
 *    This difference may be resolved in a future version of pcsc-lite.
 *    The bit-fields would then only contain one bit set.
 *    @par
 *    You can have a @b portable code using:
 *    @code
 *    if (dwState & SCARD_PRESENT)
 *    {
 *      // card is present
 *    }
 *    @endcode
 * -# \ref SCARD_E_UNSUPPORTED_FEATURE
 *    @par
 *    Windows may return ERROR_NOT_SUPPORTED instead of
 *    \ref SCARD_E_UNSUPPORTED_FEATURE
 *    @par
 *    This difference will not be corrected. pcsc-lite only uses
 *    SCARD_E_* error codes.
 * -# \ref SCARD_E_UNSUPPORTED_FEATURE
 *	  @par
 *	  For historical reasons the value of \ref SCARD_E_UNSUPPORTED_FEATURE
 *	  is \p 0x8010001F in pcsc-lite but \p 0x80100022 in Windows WinSCard.
 *	  You should not have any problem if you always use the symbolic name.
 *	  @par
 *	  The value \p 0x8010001F is also used for \ref SCARD_E_UNEXPECTED on
 *	  pcsc-lite but \ref SCARD_E_UNEXPECTED is never returned by
 *	  pcsc-lite. So \p 0x8010001F does always mean
 *	  \ref SCARD_E_UNSUPPORTED_FEATURE.
 *	  @par
 *	  Applications like rdesktop that allow a Windows application to
 *	  talk to pcsc-lite should take care of this difference and convert
 *	  the value between the two worlds.
 * -# SCardConnect()
 *    @par
 *    If \ref SCARD_SHARE_DIRECT is used the reader is accessed in
 *    shared mode (like with \ref SCARD_SHARE_SHARED) and not in
 *    exclusive mode (like with \ref SCARD_SHARE_EXCLUSIVE) as on
 *    Windows.
 * -# SCardConnect() & SCardReconnect()
 *    @par
 *    pdwActiveProtocol is not set to \ref SCARD_PROTOCOL_UNDEFINED if
 *    \ref SCARD_SHARE_DIRECT is used but the card has @b already
 *    negotiated its protocol.
 * -# SCardReconnect()
 *    @par
 *    Any PC/SC transaction held by the process is still valid after
 *    SCardReconnect() returned. On Windows the PC/SC transactions are
 *    released and a new call to SCardBeginTransaction() must be done.
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
#include <pthread.h>
#include <sys/wait.h>

#include "misc.h"
#include "pcscd.h"
#include "winscard.h"
#include "debuglog.h"

#include "readerfactory.h"
#include "eventhandler.h"
#include "sys_generic.h"
#include "winscard_msg.h"
#include "utils.h"

/* Display, on stderr, a trace of the WinSCard calls with arguments and
 * results */
//#define DO_TRACE

/* Profile the execution time of WinSCard calls */
//#define DO_PROFILE


/** used for backward compatibility */
#define SCARD_PROTOCOL_ANY_OLD	0x1000

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

static char sharing_shall_block = TRUE;

#define COLOR_RED "\33[01;31m"
#define COLOR_GREEN "\33[32m"
#define COLOR_BLUE "\33[34m"
#define COLOR_MAGENTA "\33[35m"
#define COLOR_NORMAL "\33[0m"

#ifdef DO_TRACE

#include <stdio.h>
#include <stdarg.h>

static void trace(const char *func, const char direction, const char *fmt, ...)
{
	va_list args;

	fprintf(stderr, COLOR_GREEN "%c " COLOR_BLUE "[%lX] " COLOR_GREEN "%s ",
		direction, pthread_self(), func);

	fprintf(stderr, COLOR_MAGENTA);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, COLOR_NORMAL "\n");
}

#define API_TRACE_IN(...) trace(__FUNCTION__, '<', __VA_ARGS__);
#define API_TRACE_OUT(...) trace(__FUNCTION__, '>', __VA_ARGS__);
#else
#define API_TRACE_IN(...)
#define API_TRACE_OUT(...)
#endif

#ifdef DO_PROFILE

#define PROFILE_FILE "/tmp/pcsc_profile"
#include <stdio.h>
#include <sys/time.h>

/* we can profile a maximum of 5 simultaneous calls */
#define MAX_THREADS 5
pthread_t threads[MAX_THREADS];
struct timeval profile_time_start[MAX_THREADS];
FILE *profile_fd;
char profile_tty;

#define PROFILE_START profile_start();
#define PROFILE_END(rv) profile_end(__FUNCTION__, rv);

static void profile_start(void)
{
	static char initialized = FALSE;
	pthread_t t;
	int i;

	if (!initialized)
	{
		char filename[80];

		initialized = TRUE;
		sprintf(filename, "%s-%d", PROFILE_FILE, getuid());
		profile_fd = fopen(filename, "a+");
		if (NULL == profile_fd)
		{
			fprintf(stderr, COLOR_RED "Can't open %s: %s" COLOR_NORMAL "\n",
				PROFILE_FILE, strerror(errno));
			exit(-1);
		}
		fprintf(profile_fd, "\nStart a new profile\n");

		if (isatty(fileno(stderr)))
			profile_tty = TRUE;
		else
			profile_tty = FALSE;
	}

	t = pthread_self();
	for (i=0; i<MAX_THREADS; i++)
		if (pthread_equal(0, threads[i]))
		{
			threads[i] = t;
			break;
		}

	gettimeofday(&profile_time_start[i], NULL);
} /* profile_start */

static void profile_end(const char *f, LONG rv)
{
	struct timeval profile_time_end;
	long d;
	pthread_t t;
	int i;

	gettimeofday(&profile_time_end, NULL);

	t = pthread_self();
	for (i=0; i<MAX_THREADS; i++)
		if (pthread_equal(t, threads[i]))
			break;

	if (i>=MAX_THREADS)
	{
		fprintf(stderr, COLOR_BLUE " WARNING: no start info for %s\n", f);
		return;
	}

	d = time_sub(&profile_time_end, &profile_time_start[i]);

	/* free this entry */
	threads[i] = 0;

	if (profile_tty)
	{
		if (rv != SCARD_S_SUCCESS)
			fprintf(stderr,
				COLOR_RED "RESULT %s " COLOR_MAGENTA "%ld "
				COLOR_BLUE "0x%08lX %s" COLOR_NORMAL "\n",
				f, d, rv, pcsc_stringify_error(rv));
		else
			fprintf(stderr, COLOR_RED "RESULT %s " COLOR_MAGENTA "%ld"
				COLOR_NORMAL "\n", f, d);
	}
	fprintf(profile_fd, "%s %ld\n", f, d);
	fflush(profile_fd);
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

typedef struct _psChannelMap CHANNEL_MAP;

static int CHANNEL_MAP_seeker(const void *el, const void *key)
{
	const CHANNEL_MAP * channelMap = el;

	if ((el == NULL) || (key == NULL))
	{
		Log3(PCSC_LOG_CRITICAL,
			"CHANNEL_MAP_seeker called with NULL pointer: el=%p, key=%p",
			el, key);
		return 0;
	}

	if (channelMap->hCard == *(SCARDHANDLE *)key)
		return 1;

	return 0;
}

/**
 * @brief Represents an Application Context on the Client side.
 *
 * An Application Context contains Channels (\c _psChannelMap).
 */
struct _psContextMap
{
	DWORD dwClientID;				/**< Client Connection ID */
	SCARDCONTEXT hContext;			/**< Application Context ID */
	pthread_mutex_t mMutex;			/**< Mutex for this context */
	list_t channelMapList;
	char cancellable;				/**< We are in a cancellable call */
};
/**
 * @brief Represents an Application Context on the Client side.
 *
 * typedef of _psContextMap
 */
typedef struct _psContextMap SCONTEXTMAP;

static list_t contextMapList;

static int SCONTEXTMAP_seeker(const void *el, const void *key)
{
	const SCONTEXTMAP * contextMap = el;

	if ((el == NULL) || (key == NULL))
	{
		Log3(PCSC_LOG_CRITICAL,
			"SCONTEXTMAP_seeker called with NULL pointer: el=%p, key=%p",
			el, key);
		return 0;
	}

	if (contextMap->hContext == *(SCARDCONTEXT *) key)
		return 1;

	return 0;
}

/**
 * Make sure the initialization code is executed only once.
 */
static short isExecuted = 0;


/**
 * Ensure that some functions be accessed in thread-safe mode.
 * These function's names finishes with "TH".
 */
static pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Area used to read status information about the readers.
 */
static READER_STATE readerStates[PCSCLITE_MAX_READERS_CONTEXTS];

/** Protocol Control Information for T=0 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for T=1 */
PCSC_API const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };
/** Protocol Control Information for raw access */
PCSC_API const SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, sizeof(SCARD_IO_REQUEST) };


static LONG SCardAddContext(SCARDCONTEXT, DWORD);
static SCONTEXTMAP * SCardGetAndLockContext(SCARDCONTEXT);
static SCONTEXTMAP * SCardGetContextTH(SCARDCONTEXT);
static void SCardRemoveContext(SCARDCONTEXT);
static void SCardCleanContext(SCONTEXTMAP *);

static LONG SCardAddHandle(SCARDHANDLE, SCONTEXTMAP *, LPCSTR);
static LONG SCardGetContextChannelAndLockFromHandle(SCARDHANDLE,
	/*@out@*/ SCONTEXTMAP * *, /*@out@*/ CHANNEL_MAP * *);
static LONG SCardGetContextAndChannelFromHandleTH(SCARDHANDLE,
	/*@out@*/ SCONTEXTMAP * *, /*@out@*/ CHANNEL_MAP * *);
static void SCardRemoveHandle(SCARDHANDLE);

static LONG SCardGetSetAttrib(SCARDHANDLE hCard, int command, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen);

static LONG getReaderStates(SCONTEXTMAP * currentContextMap);
static LONG getReaderStatesAndRegisterForEvents(SCONTEXTMAP * currentContextMap);
static LONG unregisterFromEvents(SCONTEXTMAP * currentContextMap);

/*
 * Thread safety functions
 */
/**
 * @brief Locks a mutex so another thread must wait to use this
 * function.
 *
 * Wrapper to the function pthread_mutex_lock().
 */
inline static void SCardLockThread(void)
{
	pthread_mutex_lock(&clientMutex);
}

/**
 * @brief Unlocks a mutex so another thread may use the client.
 *
 * Wrapper to the function pthread_mutex_unlock().
 */
inline static void SCardUnlockThread(void)
{
	pthread_mutex_unlock(&clientMutex);
}

/**
 * @brief Tell if a context index from the Application Context vector \c
 * _psContextMap is valid or not.
 *
 * @param[in] hContext Application Context whose index will be find.
 *
 * @return \c TRUE if the context exists
 * @return \c FALSE if the context does not exist
 */
static int SCardGetContextValidity(SCARDCONTEXT hContext)
{
	SCONTEXTMAP * currentContextMap;

	SCardLockThread();
	currentContextMap = SCardGetContextTH(hContext);
	SCardUnlockThread();

	return currentContextMap != NULL;
}

static LONG SCardEstablishContextTH(DWORD, LPCVOID, LPCVOID,
	/*@out@*/ LPSCARDCONTEXT);

/**
 * @brief Creates an Application Context to the PC/SC Resource Manager.
 *
 * This must be the first WinSCard function called in a PC/SC application.
 * Each thread of an application shall use its own \ref SCARDCONTEXT, unless
 * calling \ref SCardCancel(), which MUST be called with the same context as the
 * context used to call \ref SCardGetStatusChange().
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
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_PARAMETER \p phContext is null (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid scope type passed (\ref SCARD_E_INVALID_VALUE )
 * @retval SCARD_E_NO_MEMORY There is no free slot to store \p hContext (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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

	API_TRACE_IN("%ld, %p, %p", dwScope, pvReserved1, pvReserved2)
	PROFILE_START

	/* Check if the server is running */
	rv = SCardCheckDaemonAvailability();
	if (rv != SCARD_S_SUCCESS)
		goto end;

	SCardLockThread();
	rv = SCardEstablishContextTH(dwScope, pvReserved1,
		pvReserved2, phContext);
	SCardUnlockThread();

end:
	PROFILE_END(rv)
	API_TRACE_OUT("%ld", *phContext)

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
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 */
static LONG SCardEstablishContextTH(DWORD dwScope,
	/*@unused@*/ LPCVOID pvReserved1,
	/*@unused@*/ LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	LONG rv;
	struct establish_struct scEstablishStruct;
	uint32_t dwClientID = 0;

	(void)pvReserved1;
	(void)pvReserved2;
	if (phContext == NULL)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phContext = 0;

	/*
	 * Do this only once:
	 * - Initialize context list.
	 */
	if (isExecuted == 0)
	{
		int lrv;

		/* NOTE: The list will never be freed (No API call exists to
		 * "close all contexts".
		 * Applications which load and unload the library will leak
		 * the list's internal structures. */
		lrv = list_init(&contextMapList);
		if (lrv < 0)
		{
			Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %d",
				lrv);
			return SCARD_E_NO_MEMORY;
		}

		lrv = list_attributes_seeker(&contextMapList,
				SCONTEXTMAP_seeker);
		if (lrv <0)
		{
			Log2(PCSC_LOG_CRITICAL,
				"list_attributes_seeker failed with return value: %d", lrv);
			list_destroy(&contextMapList);
			return SCARD_E_NO_MEMORY;
		}

		if (getenv("PCSCLITE_NO_BLOCKING"))
		{
			Log1(PCSC_LOG_INFO, "Disable shared blocking");
			sharing_shall_block = FALSE;
		}

		isExecuted = 1;
	}


	/* Establishes a connection to the server */
	if (ClientSetupSession(&dwClientID) != 0)
	{
		return SCARD_E_NO_SERVICE;
	}

	{	/* exchange client/server protocol versions */
		struct version_struct veStr;

		veStr.major = PROTOCOL_VERSION_MAJOR;
		veStr.minor = PROTOCOL_VERSION_MINOR;
		veStr.rv = SCARD_S_SUCCESS;

		rv = MessageSendWithHeader(CMD_VERSION, dwClientID, sizeof(veStr),
			&veStr);
		if (rv != SCARD_S_SUCCESS)
			goto cleanup;

		/* Read a message from the server */
		rv = MessageReceive(&veStr, sizeof(veStr), dwClientID);
		if (rv != SCARD_S_SUCCESS)
		{
			Log1(PCSC_LOG_CRITICAL,
				"Your pcscd is too old and does not support CMD_VERSION");
			rv = SCARD_F_COMM_ERROR;
			goto cleanup;
		}

		Log3(PCSC_LOG_INFO, "Server is protocol version %d:%d",
			veStr.major, veStr.minor);

		if (veStr.rv != SCARD_S_SUCCESS)
		{
			rv = veStr.rv;
			goto cleanup;
		}
	}

again:
	/*
	 * Try to establish an Application Context with the server
	 */
	scEstablishStruct.dwScope = dwScope;
	scEstablishStruct.hContext = 0;
	scEstablishStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_ESTABLISH_CONTEXT, dwClientID,
		sizeof(scEstablishStruct), (void *) &scEstablishStruct);

	if (rv != SCARD_S_SUCCESS)
		goto cleanup;

	/*
	 * Read the response from the server
	 */
	rv = MessageReceive(&scEstablishStruct, sizeof(scEstablishStruct),
		dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto cleanup;

	if (scEstablishStruct.rv != SCARD_S_SUCCESS)
	{
		rv = scEstablishStruct.rv;
		goto cleanup;
	}

	/* check we do not reuse an existing hContext */
	if (NULL != SCardGetContextTH(scEstablishStruct.hContext))
		/* we do not need to release the allocated context since
		 * SCardReleaseContext() does nothing on the server side */
		goto again;

	*phContext = scEstablishStruct.hContext;

	/*
	 * Allocate the new hContext - if allocator full return an error
	 */
	rv = SCardAddContext(*phContext, dwClientID);

	return rv;

cleanup:
	ClientCloseSession(dwClientID);

	return rv;
}

/**
 * @brief Destroys a communication context to the PC/SC Resource
 * Manager. This must be the last function called in a PC/SC application.
 *
 * @ingroup API
 * @param[in] hContext Connection context to be closed.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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
	struct release_struct scReleaseStruct;
	SCONTEXTMAP * currentContextMap;

	API_TRACE_IN("%ld", hContext)
	PROFILE_START

	/*
	 * Make sure this context has been opened
	 * and get currentContextMap
	 */
	currentContextMap = SCardGetAndLockContext(hContext);
	if (NULL == currentContextMap)
	{
		rv = SCARD_E_INVALID_HANDLE;
		goto error;
	}

	scReleaseStruct.hContext = hContext;
	scReleaseStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_RELEASE_CONTEXT,
		currentContextMap->dwClientID,
		sizeof(scReleaseStruct), (void *) &scReleaseStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scReleaseStruct, sizeof(scReleaseStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	rv = scReleaseStruct.rv;
end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	/*
	 * Remove the local context from the stack
	 */
	SCardLockThread();
	SCardRemoveContext(hContext);
	SCardUnlockThread();

error:
	PROFILE_END(rv)
	API_TRACE_OUT("")

	return rv;
}

/**
 * @brief Establishes a connection to the reader specified in \p * szReader.
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
 *   reader. Contrary to Windows winscard behavior, the reader is accessed in
 *   shared mode and not exclusive mode.
 * @param[in] dwPreferredProtocols Desired protocol use.
 * - 0 - valid only if dwShareMode is SCARD_SHARE_DIRECT
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
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NO_SMARTCARD No smart card present (\ref SCARD_E_NO_SMARTCARD)
 * @retval SCARD_E_PROTO_MISMATCH Requested protocol is unknown (\ref SCARD_E_PROTO_MISMATCH)
 * @retval SCARD_E_READER_UNAVAILABLE Could not power up the reader or card (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 * @retval SCARD_E_UNKNOWN_READER \p szReader is NULL (\ref SCARD_E_UNKNOWN_READER)
 * @retval SCARD_E_UNSUPPORTED_FEATURE Protocol not supported (\ref SCARD_E_UNSUPPORTED_FEATURE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 * @retval SCARD_W_UNPOWERED_CARD Card is not powered (\ref SCARD_W_UNPOWERED_CARD)
 * @retval SCARD_W_UNRESPONSIVE_CARD Card is mute (\ref SCARD_W_UNRESPONSIVE_CARD)
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
	struct connect_struct scConnectStruct;
	SCONTEXTMAP * currentContextMap;

	PROFILE_START
	API_TRACE_IN("%ld %s %ld %ld", hContext, szReader, dwShareMode, dwPreferredProtocols)

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

	/*
	 * Make sure this context has been opened
	 */
	currentContextMap = SCardGetAndLockContext(hContext);
	if (NULL == currentContextMap)
		return SCARD_E_INVALID_HANDLE;

	memset(scConnectStruct.szReader, 0, sizeof scConnectStruct.szReader);
	strncpy(scConnectStruct.szReader, szReader, sizeof scConnectStruct.szReader);
	scConnectStruct.szReader[sizeof scConnectStruct.szReader -1] = '\0';

	scConnectStruct.hContext = hContext;
	scConnectStruct.dwShareMode = dwShareMode;
	scConnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scConnectStruct.hCard = 0;
	scConnectStruct.dwActiveProtocol = 0;
	scConnectStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_CONNECT, currentContextMap->dwClientID,
		sizeof(scConnectStruct), (void *) &scConnectStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scConnectStruct, sizeof(scConnectStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	*phCard = scConnectStruct.hCard;
	*pdwActiveProtocol = scConnectStruct.dwActiveProtocol;

	if (scConnectStruct.rv == SCARD_S_SUCCESS)
	{
		/*
		 * Keep track of the handle locally
		 */
		rv = SCardAddHandle(*phCard, currentContextMap, szReader);
	}
	else
		rv = scConnectStruct.rv;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)
	API_TRACE_OUT("%d", *pdwActiveProtocol)

	return rv;
}

/**
 * @brief Reestablishes a connection to a reader that was
 * previously connected to using SCardConnect().
 *
 * In a multi application environment it is possible for an application to
 * reset the card in shared mode. When this occurs any other application trying
 * to access certain commands will be returned the value \ref
 * SCARD_W_RESET_CARD. When this occurs SCardReconnect() must be called in
 * order to acknowledge that the card was reset and allow it to change its
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
 * - \ref SCARD_UNPOWER_CARD - Power down the card (cold reset).
 * - \ref SCARD_EJECT_CARD - Eject the card.
 * @param[out] pdwActiveProtocol Established protocol to this connection.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p phContext is null. (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid sharing mode, requested protocol, or reader name (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NO_SMARTCARD No smart card present (\ref SCARD_E_NO_SMARTCARD)
 * @retval SCARD_E_PROTO_MISMATCH Requested protocol is unknown (\ref SCARD_E_PROTO_MISMATCH)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_SHARING_VIOLATION Someone else has exclusive rights (\ref SCARD_E_SHARING_VIOLATION)
 * @retval SCARD_E_UNSUPPORTED_FEATURE Protocol not supported (\ref SCARD_E_UNSUPPORTED_FEATURE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
 * @retval SCARD_W_REMOVED_CARD The smart card has been removed (\ref SCARD_W_REMOVED_CARD)
 * @retval SCARD_W_UNRESPONSIVE_CARD Card is mute (\ref SCARD_W_UNRESPONSIVE_CARD)
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
	struct reconnect_struct scReconnectStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	PROFILE_START
	API_TRACE_IN("%ld %ld %ld", hCard, dwShareMode, dwPreferredProtocols)

	if (pdwActiveProtocol == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/* Retry loop for blocking behaviour */
retry:

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	scReconnectStruct.hCard = hCard;
	scReconnectStruct.dwShareMode = dwShareMode;
	scReconnectStruct.dwPreferredProtocols = dwPreferredProtocols;
	scReconnectStruct.dwInitialization = dwInitialization;
	scReconnectStruct.dwActiveProtocol = *pdwActiveProtocol;
	scReconnectStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_RECONNECT, currentContextMap->dwClientID,
		sizeof(scReconnectStruct), (void *) &scReconnectStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scReconnectStruct, sizeof(scReconnectStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	rv = scReconnectStruct.rv;

	if (sharing_shall_block && (SCARD_E_SHARING_VIOLATION == rv))
	{
		(void)pthread_mutex_unlock(&currentContextMap->mMutex);
		(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		goto retry;
	}

	*pdwActiveProtocol = scReconnectStruct.dwActiveProtocol;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)
	API_TRACE_OUT("%ld", *pdwActiveProtocol)

	return rv;
}

/**
 * @brief Terminates a connection made through SCardConnect().
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwDisposition Reader function to execute.
 * - \ref SCARD_LEAVE_CARD - Do nothing.
 * - \ref SCARD_RESET_CARD - Reset the card (warm reset).
 * - \ref SCARD_UNPOWER_CARD - Power down the card (cold reset).
 * - \ref SCARD_EJECT_CARD - Eject the card.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful(\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_VALUE Invalid \p dwDisposition (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NO_SMARTCARD No smart card present (\ref SCARD_E_NO_SMARTCARD)
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
	struct disconnect_struct scDisconnectStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	PROFILE_START
	API_TRACE_IN("%ld %ld", hCard, dwDisposition)

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
	{
		rv = SCARD_E_INVALID_HANDLE;
		goto error;
	}

	scDisconnectStruct.hCard = hCard;
	scDisconnectStruct.dwDisposition = dwDisposition;
	scDisconnectStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_DISCONNECT, currentContextMap->dwClientID,
		sizeof(scDisconnectStruct), (void *) &scDisconnectStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scDisconnectStruct, sizeof(scDisconnectStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	if (SCARD_S_SUCCESS == scDisconnectStruct.rv)
		SCardRemoveHandle(hCard);
	rv = scDisconnectStruct.rv;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

error:
	PROFILE_END(rv)
	API_TRACE_OUT("")

	return rv;
}

/**
 * @brief Establishes a temporary exclusive access mode for
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
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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
	struct begin_struct scBeginStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	PROFILE_START
	API_TRACE_IN("%ld", hCard)

	/*
	 * Query the server every so often until the sharing violation ends
	 * and then hold the lock for yourself.
	 */

	for(;;)
	{
		/*
		 * Make sure this handle has been opened
		 */
		rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
			&pChannelMap);
		if (rv == -1)
			return SCARD_E_INVALID_HANDLE;

		scBeginStruct.hCard = hCard;
		scBeginStruct.rv = SCARD_S_SUCCESS;

		rv = MessageSendWithHeader(SCARD_BEGIN_TRANSACTION,
			currentContextMap->dwClientID,
			sizeof(scBeginStruct), (void *) &scBeginStruct);

		if (rv != SCARD_S_SUCCESS)
			break;

		/*
		 * Read a message from the server
		 */
		rv = MessageReceive(&scBeginStruct, sizeof(scBeginStruct),
			currentContextMap->dwClientID);

		if (rv != SCARD_S_SUCCESS)
			break;

		rv = scBeginStruct.rv;

		if (SCARD_E_SHARING_VIOLATION != rv)
			break;

		(void)pthread_mutex_unlock(&currentContextMap->mMutex);
		(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
	}

	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)
	API_TRACE_OUT("")

	return rv;
}

/**
 * @brief Ends a previously begun transaction.
 *
 * The calling application must be the owner of the previously begun
 * transaction or an error will occur.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwDisposition Action to be taken on the reader.
 * - \ref SCARD_LEAVE_CARD - Do nothing.
 * - \ref SCARD_RESET_CARD - Reset the card.
 * - \ref SCARD_UNPOWER_CARD - Power down the card.
 * - \ref SCARD_EJECT_CARD - Eject the card.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_VALUE Invalid value for \p dwDisposition (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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
	struct end_struct scEndStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	PROFILE_START
	API_TRACE_IN("%ld", hCard)

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	scEndStruct.hCard = hCard;
	scEndStruct.dwDisposition = dwDisposition;
	scEndStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_END_TRANSACTION,
		currentContextMap->dwClientID,
		sizeof(scEndStruct), (void *) &scEndStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scEndStruct, sizeof(scEndStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	rv = scEndStruct.rv;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)
	API_TRACE_OUT("")

	return rv;
}

/**
 * @brief Returns the current status of the reader connected to
 * by \p hCard.
 *
 * Its friendly name will be stored in \p szReaderName. \p pcchReaderLen will
 * be the size of the allocated buffer for \p szReaderName, while \p pcbAtrLen
 * will be the size of the allocated buffer for \p pbAtr. If either of these is
 * too small, the function will return with \ref SCARD_E_INSUFFICIENT_BUFFER
 * and the necessary size in \p pcchReaderLen and \p pcbAtrLen. The current
 * state, and protocol will be stored in pdwState and \p pdwProtocol
 * respectively.
 *
 * *pdwState also contains a number of events in the upper 16 bits
 * (*pdwState & 0xFFFF0000). This number of events is incremented
 * for each card insertion or removal in the specified reader. This can
 * be used to detect a card removal/insertion between two calls to
 * SCardStatus().
 *
 * If \c *pcchReaderLen is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory for szReaderName. Use
 * SCardFreeMemory() to release it.
 *
 * If \c *pcbAtrLen is equal to \ref SCARD_AUTOALLOCATE then the function will
 * allocate itself the needed memory for pbAtr. Use SCardFreeMemory() to
 * release it.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in,out] szReaderName Friendly name of this reader.
 * @param[in,out] pcchReaderLen Size of the \p szReaderName.
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
 * - \ref SCARD_PROTOCOL_T0	Use the T=0 protocol.
 * - \ref SCARD_PROTOCOL_T1	Use the T=1 protocol.
 * @param[out] pbAtr Current ATR of a card in this reader.
 * @param[out] pcbAtrLen Length of ATR.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Not enough allocated memory for \p szReaderName or for \p pbAtr (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pcchReaderLen or \p pcbAtrLen is NULL (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_MEMORY Memory allocation failed (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 * @retval SCARD_F_INTERNAL_ERROR An internal consistency check failed (\ref SCARD_F_INTERNAL_ERROR)
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
LONG SCardStatus(SCARDHANDLE hCard, LPSTR szReaderName,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	DWORD dwReaderLen, dwAtrLen;
	LONG rv;
	int i;
	struct status_struct scStatusStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;
	char *r;
	char *bufReader = NULL;
	LPBYTE bufAtr = NULL;
	DWORD dummy = 0;

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

	/* Retry loop for blocking behaviour */
retry:

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	/* synchronize reader states with daemon */
	rv = getReaderStates(currentContextMap);
	if (rv != SCARD_S_SUCCESS)
		goto end;

	r = pChannelMap->readerName;
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
	{
		/* by default r == NULL */
		if (r && strcmp(r, readerStates[i].readerName) == 0)
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

	rv = MessageSendWithHeader(SCARD_STATUS, currentContextMap->dwClientID,
		sizeof(scStatusStruct), (void *) &scStatusStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scStatusStruct, sizeof(scStatusStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	rv = scStatusStruct.rv;

	if (sharing_shall_block && (SCARD_E_SHARING_VIOLATION == rv))
	{
		(void)pthread_mutex_unlock(&currentContextMap->mMutex);
		(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		goto retry;
	}

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

	*pcchReaderLen = strlen(pChannelMap->readerName) + 1;
	*pcbAtrLen = readerStates[i].cardAtrLength;

	if (pdwState)
		*pdwState = (readerStates[i].eventCounter << 16) + readerStates[i].readerState;

	if (pdwProtocol)
		*pdwProtocol = readerStates[i].cardProtocol;

	if (SCARD_AUTOALLOCATE == dwReaderLen)
	{
		dwReaderLen = *pcchReaderLen;
		if (NULL == szReaderName)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		bufReader = malloc(dwReaderLen);
		if (NULL == bufReader)
		{
			rv = SCARD_E_NO_MEMORY;
			goto end;
		}
		*(char **)szReaderName = bufReader;
	}
	else
		bufReader = szReaderName;

	/* return SCARD_E_INSUFFICIENT_BUFFER only if buffer pointer is non NULL */
	if (bufReader)
	{
		if (*pcchReaderLen > dwReaderLen)
			rv = SCARD_E_INSUFFICIENT_BUFFER;

		strncpy(bufReader, pChannelMap->readerName, dwReaderLen);
	}

	if (SCARD_AUTOALLOCATE == dwAtrLen)
	{
		dwAtrLen = *pcbAtrLen;
		if (NULL == pbAtr)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		bufAtr = malloc(dwAtrLen);
		if (NULL == bufAtr)
		{
			rv = SCARD_E_NO_MEMORY;
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

		memcpy(bufAtr, readerStates[i].cardAtr, min(*pcbAtrLen, dwAtrLen));
	}

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief Blocks execution until the current availability
 * of the cards in a specific set of readers changes.
 *
 * This function receives a structure or list of structures containing
 * reader names. It then blocks waiting for a change in state to occur
 * for a maximum blocking time of \p dwTimeout or forever if \ref
 * INFINITE is used.
 *
 * The new event state will be contained in \p dwEventState. A status change
 * might be a card insertion or removal event, a change in ATR, etc.
 *
 * \p dwEventState also contains a number of events in the upper 16 bits
 * (\p dwEventState & 0xFFFF0000). This number of events is incremented
 * for each card insertion or removal in the specified reader. This can
 * be used to detect a card removal/insertion between two calls to
 * SCardGetStatusChange()
 *
 * To wait for a reader event (reader added or removed) you may use the special
 * reader name \c "\\?PnP?\Notification". If a reader event occurs the state of
 * this reader will change and the bit \ref SCARD_STATE_CHANGED will be set.
 *
 * To cancel the ongoing call, use \ref SCardCancel() with the same
 * \ref SCARDCONTEXT.
 *
 * @code
 * typedef struct {
 *   LPCSTR szReader;           // Reader name
 *   LPVOID pvUserData;         // User defined data
 *   DWORD dwCurrentState;      // Current state of reader
 *   DWORD dwEventState;        // Reader state after a state change
 *   DWORD cbAtr;               // ATR Length, usually MAX_ATR_SIZE
 *   BYTE rgbAtr[MAX_ATR_SIZE]; // ATR Value
 * } SCARD_READERSTATE, *LPSCARD_READERSTATE;
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
 * @param[in] dwTimeout Maximum waiting time (in milliseconds) for status
 *            change, \ref INFINITE for infinite.
 * @param[in,out] rgReaderStates Structures of readers with current states.
 * @param[in] cReaders Number of structures.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_NO_SERVICE Server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_INVALID_PARAMETER \p rgReaderStates is NULL and \p cReaders > 0 (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid States, reader name, etc (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_INVALID_HANDLE Invalid hContext handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader is unavailable (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_UNKNOWN_READER The reader name is unknown (\ref SCARD_E_UNKNOWN_READER)
 * @retval SCARD_E_TIMEOUT The user-specified timeout value has expired (\ref SCARD_E_TIMEOUT)
 * @retval SCARD_E_CANCELLED The call has been cancelled by a call to
 * SCardCancel() (\ref SCARD_E_CANCELLED)
 *
 * @code
 * SCARDCONTEXT hContext;
 * SCARD_READERSTATE rgReaderStates[2];
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
 * // Get current state
 * rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 2);
 * printf("reader state: 0x%04X\n", rgReaderStates[0].dwEventState);
 * printf("reader state: 0x%04X\n", rgReaderStates[1].dwEventState);
 *
 * // Wait for card insertion
 * if (rgReaderStates[0].dwEventState & SCARD_STATE_EMPTY)
 * {
 *     rgReaderStates[0].dwCurrentState = rgReaderStates[0].dwEventState;
 *     rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 2);
 * }
 * @endcode
 */
LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	SCARD_READERSTATE *rgReaderStates, DWORD cReaders)
{
	SCARD_READERSTATE *currReader;
	READER_STATE *rContext;
	long dwTime;
	DWORD dwBreakFlag = 0;
	unsigned int j;
	SCONTEXTMAP * currentContextMap;
	int currentReaderCount = 0;
	LONG rv = SCARD_S_SUCCESS;

	PROFILE_START
	API_TRACE_IN("%ld %ld %d", hContext, dwTimeout, cReaders)
#ifdef DO_TRACE
	for (j=0; j<cReaders; j++)
	{
		API_TRACE_IN("[%d] %s %lX %lX", j, rgReaderStates[j].szReader,
			rgReaderStates[j].dwCurrentState, rgReaderStates[j].dwEventState)
	}
#endif

	if ((rgReaderStates == NULL && cReaders > 0)
		|| (cReaders > PCSCLITE_MAX_READERS_CONTEXTS))
	{
		rv = SCARD_E_INVALID_PARAMETER;
		goto error;
	}

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
		{
			rv = SCARD_S_SUCCESS;
			goto error;
		}
	}
	else
	{
		/* reader list is empty */
		rv = SCARD_S_SUCCESS;
		goto error;
	}

	/*
	 * Make sure this context has been opened
	 */
	currentContextMap = SCardGetAndLockContext(hContext);
	if (NULL == currentContextMap)
	{
		rv = SCARD_E_INVALID_HANDLE;
		goto error;
	}

	/* synchronize reader states with daemon */
	rv = getReaderStatesAndRegisterForEvents(currentContextMap);
	if (rv != SCARD_S_SUCCESS)
		goto end;

	/* check all the readers are already known */
	for (j=0; j<cReaders; j++)
	{
		const char *readerName;
		int i;

		readerName = rgReaderStates[j].szReader;
		for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		{
			if (strcmp(readerName, readerStates[i].readerName) == 0)
				break;
		}

		/* The requested reader name is not recognized */
		if (i == PCSCLITE_MAX_READERS_CONTEXTS)
		{
			/* PnP special reader? */
			if (strcasecmp(readerName, "\\\\?PnP?\\Notification") != 0)
			{
				rv = SCARD_E_UNKNOWN_READER;
				goto end;
			}
		}
	}

	/* Clear the event state for all readers */
	for (j = 0; j < cReaders; j++)
		rgReaderStates[j].dwEventState = 0;

	/* Now is where we start our event checking loop */
	Log2(PCSC_LOG_DEBUG, "Event Loop Start, dwTimeout: %ld", dwTimeout);

	/* Get the initial reader count on the system */
	for (j=0; j < PCSCLITE_MAX_READERS_CONTEXTS; j++)
		if (readerStates[j].readerName[0] != '\0')
			currentReaderCount++;

	/* catch possible sign extension problems from 32 to 64-bits integers */
	if ((DWORD)-1 == dwTimeout)
		dwTimeout = INFINITE;
	if (INFINITE == dwTimeout)
		dwTime = 60*1000;	/* "infinite" timeout */
	else
		dwTime = dwTimeout;

	j = 0;
	do
	{
		currReader = &rgReaderStates[j];

		/* Ignore for IGNORED readers */
		if (!(currReader->dwCurrentState & SCARD_STATE_IGNORE))
		{
			const char *readerName;
			int i;

			/* Looks for correct readernames */
			readerName = currReader->szReader;
			for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
			{
				if (strcmp(readerName, readerStates[i].readerName) == 0)
					break;
			}

			/* The requested reader name is not recognized */
			if (i == PCSCLITE_MAX_READERS_CONTEXTS)
			{
				/* PnP special reader? */
				if (strcasecmp(readerName, "\\\\?PnP?\\Notification") == 0)
				{
					int k, newReaderCount = 0;

					for (k=0; k < PCSCLITE_MAX_READERS_CONTEXTS; k++)
						if (readerStates[k].readerName[0] != '\0')
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
					currReader->dwEventState =
						SCARD_STATE_UNKNOWN | SCARD_STATE_UNAVAILABLE;
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
				uint32_t readerState;

				/* The reader has come back after being away */
				if (currReader->dwCurrentState & SCARD_STATE_UNKNOWN)
				{
					currReader->dwEventState |= SCARD_STATE_CHANGED;
					currReader->dwEventState &= ~SCARD_STATE_UNKNOWN;
					Log0(PCSC_LOG_DEBUG);
					dwBreakFlag = 1;
				}

				/* Set the reader status structure */
				rContext = &readerStates[i];

				/* Now we check all the Reader States */
				readerState = rContext->readerState;

				/* only if current state has an non null event counter */
				if (currReader->dwCurrentState & 0xFFFF0000)
				{
					unsigned int currentCounter;

					currentCounter = (currReader->dwCurrentState >> 16) & 0xFFFF;

					/* has the event counter changed since the last call? */
					if (rContext->eventCounter != currentCounter)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}
				}

				/* add an event counter in the upper word of dwEventState */
				currReader->dwEventState = ((currReader->dwEventState & 0xffff )
					| (rContext->eventCounter << 16));

				/* Check if the reader is in the correct state */
				if (readerState & SCARD_UNKNOWN)
				{
					/* reader is in bad state */
					currReader->dwEventState = SCARD_STATE_UNAVAILABLE;
					if (!(currReader->dwCurrentState & SCARD_STATE_UNAVAILABLE))
					{
						/* App thinks reader is in good state and it is not */
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						Log0(PCSC_LOG_DEBUG);
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
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}
				}

				/* Check for card presence in the reader */
				if (readerState & SCARD_PRESENT)
				{
					/* card present but not yet powered up */
					if (0 == rContext->cardAtrLength)
						/* Allow the status thread to convey information */
						(void)SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

					currReader->cbAtr = rContext->cardAtrLength;
					memcpy(currReader->rgbAtr, rContext->cardAtr,
						currReader->cbAtr);
				}
				else
					currReader->cbAtr = 0;

				/* Card is now absent */
				if (readerState & SCARD_ABSENT)
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
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}
				}
				/* Card is now present */
				else if (readerState & SCARD_PRESENT)
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
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}

					if (readerState & SCARD_SWALLOWED)
					{
						currReader->dwEventState |= SCARD_STATE_MUTE;
						if (!(currReader->dwCurrentState & SCARD_STATE_MUTE))
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							Log0(PCSC_LOG_DEBUG);
							dwBreakFlag = 1;
						}
					}
					else
					{
						/* App thinks card is mute but it is not */
						if (currReader->dwCurrentState & SCARD_STATE_MUTE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							Log0(PCSC_LOG_DEBUG);
							dwBreakFlag = 1;
						}
					}
				}

				/* Now figure out sharing modes */
				if (rContext->readerSharing == PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
				{
					currReader->dwEventState |= SCARD_STATE_EXCLUSIVE;
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}
				}
				else if (rContext->readerSharing >= PCSCLITE_SHARING_LAST_CONTEXT)
				{
					/* A card must be inserted for it to be INUSE */
					if (readerState & SCARD_PRESENT)
					{
						currReader->dwEventState |= SCARD_STATE_INUSE;
						currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;
						if (currReader-> dwCurrentState & SCARD_STATE_EXCLUSIVE)
						{
							currReader->dwEventState |= SCARD_STATE_CHANGED;
							Log0(PCSC_LOG_DEBUG);
							dwBreakFlag = 1;
						}
					}
				}
				else if (rContext->readerSharing == PCSCLITE_SHARING_NO_CONTEXT)
				{
					currReader->dwEventState &= ~SCARD_STATE_INUSE;
					currReader->dwEventState &= ~SCARD_STATE_EXCLUSIVE;

					if (currReader->dwCurrentState & SCARD_STATE_INUSE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						Log0(PCSC_LOG_DEBUG);
						dwBreakFlag = 1;
					}
					else if (currReader-> dwCurrentState
						& SCARD_STATE_EXCLUSIVE)
					{
						currReader->dwEventState |= SCARD_STATE_CHANGED;
						Log0(PCSC_LOG_DEBUG);
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
					Log0(PCSC_LOG_DEBUG);
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

			/* Only sleep once for each cycle of reader checks. */
			{
				struct wait_reader_state_change waitStatusStruct = {0};
				struct timeval before, after;

				gettimeofday(&before, NULL);

				waitStatusStruct.rv = SCARD_S_SUCCESS;

				/* another thread can do SCardCancel() */
				currentContextMap->cancellable = TRUE;

				/*
				 * Read a message from the server
				 */
				rv = MessageReceiveTimeout(CMD_WAIT_READER_STATE_CHANGE,
					&waitStatusStruct, sizeof(waitStatusStruct),
					currentContextMap->dwClientID, dwTime);

				/* SCardCancel() will return immediatly with success
				 * because something changed on the daemon side. */
				currentContextMap->cancellable = FALSE;

				/* timeout */
				if (SCARD_E_TIMEOUT == rv)
				{
					/* ask server to remove us from the event list */
					rv = unregisterFromEvents(currentContextMap);
				}

				if (rv != SCARD_S_SUCCESS)
					goto end;

				/* an event occurs or SCardCancel() was called */
				if (SCARD_S_SUCCESS != waitStatusStruct.rv)
				{
					rv = waitStatusStruct.rv;
					goto end;
				}

				/* synchronize reader states with daemon */
				rv = getReaderStatesAndRegisterForEvents(currentContextMap);
				if (rv != SCARD_S_SUCCESS)
					goto end;

				if (INFINITE != dwTimeout)
				{
					long int diff;

					gettimeofday(&after, NULL);
					diff = time_sub(&after, &before);
					dwTime -= diff/1000;
				}
			}

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

end:
	Log1(PCSC_LOG_DEBUG, "Event Loop End");

	/* if SCardCancel() has been used then the client is already
	 * unregistered */
	if (SCARD_E_CANCELLED != rv)
		(void)unregisterFromEvents(currentContextMap);

	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

error:
	PROFILE_END(rv)
#ifdef DO_TRACE
	for (j=0; j<cReaders; j++)
	{
		API_TRACE_OUT("[%d] %s %X %X", j, rgReaderStates[j].szReader,
			rgReaderStates[j].dwCurrentState, rgReaderStates[j].dwEventState)
	}
#endif

	return rv;
}

/**
 * @brief Sends a command directly to the IFD Handler (reader
 * driver) to be processed by the reader.
 *
 * This is useful for creating client side reader drivers for functions like
 * PIN pads, biometrics, or other extensions to the normal smart card reader
 * that are not normally handled by PC/SC.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwControlCode Control code for the operation.\n
 * <a href="http://anonscm.debian.org/viewvc/pcsclite/trunk/Drivers/ccid/SCARDCONTOL.txt?view=markup">
 * Click here</a> for a list of supported commands by some drivers.
 * @param[in] pbSendBuffer Command to send to the reader.
 * @param[in] cbSendLength Length of the command.
 * @param[out] pbRecvBuffer Response from the reader.
 * @param[in] cbRecvLength Length of the response buffer.
 * @param[out] lpBytesReturned Length of the response.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER \p cbRecvLength was not large enough for the reader response. The expected size is now in \p lpBytesReturned (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pbSendBuffer is NULL or \p cbSendLength is null and the IFDHandler is version 2.0 (without \p dwControlCode) (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid value was presented (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed(\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_E_UNSUPPORTED_FEATURE Driver does not support (\ref SCARD_E_UNSUPPORTED_FEATURE)
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
 * BYTE pbSendBuffer[] = { 0x06, 0x00, 0x0A, 0x01, 0x01, 0x10, 0x00 };
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
	struct control_struct scControlStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	PROFILE_START

	/* 0 bytes received by default */
	if (NULL != lpBytesReturned)
		*lpBytesReturned = 0;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
	{
		PROFILE_END(SCARD_E_INVALID_HANDLE)
		return SCARD_E_INVALID_HANDLE;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE_EXTENDED)
		|| (cbRecvLength > MAX_BUFFER_SIZE_EXTENDED))
	{
		rv = SCARD_E_INSUFFICIENT_BUFFER;
		goto end;
	}

	scControlStruct.hCard = hCard;
	scControlStruct.dwControlCode = dwControlCode;
	scControlStruct.cbSendLength = cbSendLength;
	scControlStruct.cbRecvLength = cbRecvLength;
	scControlStruct.dwBytesReturned = 0;
	scControlStruct.rv = 0;

	rv = MessageSendWithHeader(SCARD_CONTROL, currentContextMap->dwClientID,
		sizeof(scControlStruct), &scControlStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/* write the sent buffer */
	rv = MessageSend((char *)pbSendBuffer, cbSendLength,
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scControlStruct, sizeof(scControlStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	if (SCARD_S_SUCCESS == scControlStruct.rv)
	{
		/* read the received buffer */
		rv = MessageReceive(pbRecvBuffer, scControlStruct.dwBytesReturned,
			currentContextMap->dwClientID);

		if (rv != SCARD_S_SUCCESS)
			goto end;

	}

	if (NULL != lpBytesReturned)
		*lpBytesReturned = scControlStruct.dwBytesReturned;

	rv = scControlStruct.rv;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief Get an attribute from the IFD Handler (reader driver).
 *
 * The list of possible attributes is available in the file \c reader.h.
 *
 * If \c *pcbAttrLen is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] dwAttrId Identifier for the attribute to get.\n
 * Not all the \p dwAttrId values listed above may be implemented in the IFD
 * Handler you are using. And some \p dwAttrId values not listed here may be
 * implemented.
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
 *   Implemented by pcsc-lite if the IFD Handler (driver) returns \ref
 *   IFD_ERROR_TAG.  pcsc-lite then returns the same reader name as
 *   returned by \ref SCardListReaders().
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
 * @param[out] pbAttr Pointer to a buffer that receives the attribute.
 * If this value is NULL, SCardGetAttrib() ignores the buffer length
 * supplied in \p pcbAttrLen, writes the length of the buffer that would
 * have been returned if this parameter had not been NULL to \p pcbAttrLen,
 * and returns a success code.
 * @param[in,out] pcbAttrLen Length of the \p pbAttr buffer in bytes and receives the actual length of the received attribute.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_UNSUPPORTED_FEATURE the \p dwAttrId attribute is not supported by the driver (\ref SCARD_E_UNSUPPORTED_FEATURE)
 * @retval SCARD_E_NOT_TRANSACTED
 * - the driver returned an error (\ref SCARD_E_NOT_TRANSACTED)
 * - Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_INSUFFICIENT_BUFFER
 * - \p cbAttrLen is too big (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * - \p pbAttr buffer not large enough. In that case the expected buffer size is indicated in \p *pcbAttrLen (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER A parameter is NULL and should not (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_MEMORY Memory allocation failed (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
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
 * rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, NULL, &dwAttrLen);
 * if (SCARD_S_SUCCESS == rv)
 * {
 *     pbAttr = malloc(dwAttrLen);
 *     rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, pbAttr, &dwAttrLen);
 *     free(pbAttr);
 * }
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
	{
		ret = SCARD_E_INVALID_PARAMETER;
		goto end;
	}

	if (SCARD_AUTOALLOCATE == *pcbAttrLen)
	{
		if (NULL == pbAttr)
			return SCARD_E_INVALID_PARAMETER;

		*pcbAttrLen = MAX_BUFFER_SIZE;
		buf = malloc(*pcbAttrLen);
		if (NULL == buf)
		{
			ret = SCARD_E_NO_MEMORY;
			goto end;
		}

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

end:
	PROFILE_END(ret)

	return ret;
}

/**
 * @brief Set an attribute of the IFD Handler.
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
 * @retval SCARD_E_INSUFFICIENT_BUFFER \p cbAttrLen is too big (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER A parameter is NULL and should not (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_E_NOT_TRANSACTED Data exchange not successful (\ref SCARD_E_NOT_TRANSACTED)
 * @retval SCARD_E_READER_UNAVAILABLE The reader has been removed (\ref SCARD_E_READER_UNAVAILABLE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * LONG rv;
 * SCARDCONTEXT hContext;
 * SCARDHANDLE hCard;
 * DWORD dwActiveProtocol;
 * unsigned char pbAttr[] = { 0x12, 0x34, 0x56 };
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_RAW, &hCard, &dwActiveProtocol);
 * rv = SCardSetAttrib(hCard, 0x42000001, pbAttr, sizeof(pbAttr));
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
	struct getset_struct scGetSetStruct;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
		return SCARD_E_INVALID_HANDLE;

	if (*pcbAttrLen > MAX_BUFFER_SIZE)
	{
		rv = SCARD_E_INSUFFICIENT_BUFFER;
		goto end;
	}

	scGetSetStruct.hCard = hCard;
	scGetSetStruct.dwAttrId = dwAttrId;
	scGetSetStruct.rv = SCARD_E_NO_SERVICE;
	memset(scGetSetStruct.pbAttr, 0, sizeof(scGetSetStruct.pbAttr));
	if (SCARD_SET_ATTRIB == command)
	{
		memcpy(scGetSetStruct.pbAttr, pbAttr, *pcbAttrLen);
		scGetSetStruct.cbAttrLen = *pcbAttrLen;
	}
	else
		/* we can get up to the communication buffer size */
		scGetSetStruct.cbAttrLen = sizeof scGetSetStruct.pbAttr;

	rv = MessageSendWithHeader(command, currentContextMap->dwClientID,
		sizeof(scGetSetStruct), &scGetSetStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scGetSetStruct, sizeof(scGetSetStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	if ((SCARD_S_SUCCESS == scGetSetStruct.rv) && (SCARD_GET_ATTRIB == command))
	{
		/*
		 * Copy and zero it so any secret information is not leaked
		 */
		if (*pcbAttrLen < scGetSetStruct.cbAttrLen)
		{
			/* restrict the value of scGetSetStruct.cbAttrLen to avoid a
			 * buffer overflow in the memcpy() bellow */
			DWORD correct_value = scGetSetStruct.cbAttrLen;
			scGetSetStruct.cbAttrLen = *pcbAttrLen;
			*pcbAttrLen = correct_value;

			scGetSetStruct.rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
		else
			*pcbAttrLen = scGetSetStruct.cbAttrLen;

		if (pbAttr)
			memcpy(pbAttr, scGetSetStruct.pbAttr, scGetSetStruct.cbAttrLen);

		memset(scGetSetStruct.pbAttr, 0x00, sizeof(scGetSetStruct.pbAttr));
	}
	rv = scGetSetStruct.rv;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	return rv;
}

/**
 * @brief Sends an APDU to the smart card contained in the reader
 * connected to by SCardConnect().
 *
 * The card responds from the APDU and stores this response in \p pbRecvBuffer
 * and its length in \p pcbRecvLength.
 *
 * \p pioSendPci and \p pioRecvPci are structures containing the following:
 * @code
 * typedef struct {
 *    DWORD dwProtocol;    // SCARD_PROTOCOL_T0, SCARD_PROTOCOL_T1 or SCARD_PROTOCOL_RAW
 *    DWORD cbPciLength;   // Length of this structure
 * } SCARD_IO_REQUEST;
 * @endcode
 *
 * @ingroup API
 * @param[in] hCard Connection made from SCardConnect().
 * @param[in] pioSendPci Structure of Protocol Control Information.
 * - \ref SCARD_PCI_T0 - Predefined T=0 PCI structure.
 * - \ref SCARD_PCI_T1 - Predefined T=1 PCI structure.
 * - \ref SCARD_PCI_RAW - Predefined RAW PCI structure.
 * @param[in] pbSendBuffer APDU to send to the card.
 * @param[in] cbSendLength Length of the APDU.
 * @param[in,out] pioRecvPci Structure of protocol information. This parameter can be NULL if no PCI is returned.
 * @param[out] pbRecvBuffer Response from the card.
 * @param[in,out] pcbRecvLength Length of the response.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER \p cbRecvLength was not large enough for the card response. The expected size is now in \p cbRecvLength (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hCard handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pbSendBuffer or \p pbRecvBuffer or \p pcbRecvLength or \p pioSendPci is null (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_INVALID_VALUE Invalid Protocol, reader name, etc (\ref SCARD_E_INVALID_VALUE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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
 * BYTE pbRecvBuffer[10];
 * BYTE pbSendBuffer[] = { 0xC0, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardConnect(hContext, "Reader X", SCARD_SHARE_SHARED,
 *          SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
 * dwSendLength = sizeof(pbSendBuffer);
 * dwRecvLength = sizeof(pbRecvBuffer);
 * rv = SCardTransmit(hCard, SCARD_PCI_T0, pbSendBuffer, dwSendLength,
 *          NULL, pbRecvBuffer, &dwRecvLength);
 * @endcode
 */
LONG SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST *pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	SCARD_IO_REQUEST *pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * pChannelMap;
	struct transmit_struct scTransmitStruct;

	PROFILE_START

	if (pbSendBuffer == NULL || pbRecvBuffer == NULL ||
			pcbRecvLength == NULL || pioSendPci == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/* Retry loop for blocking behaviour */
retry:

	/*
	 * Make sure this handle has been opened
	 */
	rv = SCardGetContextChannelAndLockFromHandle(hCard, &currentContextMap,
		&pChannelMap);
	if (rv == -1)
	{
		*pcbRecvLength = 0;
		PROFILE_END(SCARD_E_INVALID_HANDLE)
		return SCARD_E_INVALID_HANDLE;
	}

	if ((cbSendLength > MAX_BUFFER_SIZE_EXTENDED)
		|| (*pcbRecvLength > MAX_BUFFER_SIZE_EXTENDED))
	{
		rv = SCARD_E_INSUFFICIENT_BUFFER;
		goto end;
	}

	scTransmitStruct.hCard = hCard;
	scTransmitStruct.cbSendLength = cbSendLength;
	scTransmitStruct.pcbRecvLength = *pcbRecvLength;
	scTransmitStruct.ioSendPciProtocol = pioSendPci->dwProtocol;
	scTransmitStruct.ioSendPciLength = pioSendPci->cbPciLength;
	scTransmitStruct.rv = SCARD_S_SUCCESS;

	if (pioRecvPci)
	{
		scTransmitStruct.ioRecvPciProtocol = pioRecvPci->dwProtocol;
		scTransmitStruct.ioRecvPciLength = pioRecvPci->cbPciLength;
	}
	else
	{
		scTransmitStruct.ioRecvPciProtocol = SCARD_PROTOCOL_ANY;
		scTransmitStruct.ioRecvPciLength = sizeof(SCARD_IO_REQUEST);
	}

	rv = MessageSendWithHeader(SCARD_TRANSMIT, currentContextMap->dwClientID,
		sizeof(scTransmitStruct), (void *) &scTransmitStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/* write the sent buffer */
	rv = MessageSend((void *)pbSendBuffer, cbSendLength,
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scTransmitStruct, sizeof(scTransmitStruct),
		currentContextMap->dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	if (SCARD_S_SUCCESS == scTransmitStruct.rv)
	{
		/* read the received buffer */
		rv = MessageReceive(pbRecvBuffer, scTransmitStruct.pcbRecvLength,
			currentContextMap->dwClientID);

		if (rv != SCARD_S_SUCCESS)
			goto end;

		if (pioRecvPci)
		{
			pioRecvPci->dwProtocol = scTransmitStruct.ioRecvPciProtocol;
			pioRecvPci->cbPciLength = scTransmitStruct.ioRecvPciLength;
		}
	}

	rv = scTransmitStruct.rv;

	if (sharing_shall_block && (SCARD_E_SHARING_VIOLATION == rv))
	{
		(void)pthread_mutex_unlock(&currentContextMap->mMutex);
		(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		goto retry;
	}

	*pcbRecvLength = scTransmitStruct.pcbRecvLength;

end:
	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * Returns a list of currently available readers on the system.
 *
 * \p mszReaders is a pointer to a character string that is allocated by the
 * application.  If the application sends \p mszGroups and \p mszReaders as
 * NULL then this function will return the size of the buffer needed to
 * allocate in \p pcchReaders.
 *
 * If \c *pcchReaders is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * Encoding:
 * The reader names and group names are encoded using UTF-8.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[in] mszGroups List of groups to list readers (not used).
 * @param[out] mszReaders Multi-string with list of readers.
 * @param[in,out] pcchReaders Size of multi-string buffer including NULL's.
 *
 * @return Connection status.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Reader buffer not large enough (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid Scope Handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p pcchReaders is NULL (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_MEMORY Memory allocation failed (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_READERS_AVAILABLE No readers available (\ref SCARD_E_NO_READERS_AVAILABLE)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszReaders;
 * DWORD dwReaders;
 * LONG rv;
 *
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
 * mszReaders = malloc(sizeof(char)*dwReaders);
 * rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
 *
 * char *p = mszReaders;
 * while (*p)
 * {
 *	 printf("Reader: %s\n", p);
 *	 p += strlen(p) +1;
 * }
 * @endcode
 *
 * or, with auto allocation:
 *
 * @code
 * SCARDCONTEXT hContext;
 * LPSTR mszReaders;
 * DWORD dwReaders;
 * LONG rv;
 *
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * dwReaders = SCARD_AUTOALLOCATE;
 * rv = SCardListReaders(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
 * rv = SCardFreeMemory(hContext, mszReaders);
 * @endcode
 */
LONG SCardListReaders(SCARDCONTEXT hContext, /*@unused@*/ LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	DWORD dwReadersLen = 0;
	int i;
	SCONTEXTMAP * currentContextMap;
	LONG rv = SCARD_S_SUCCESS;
	char *buf = NULL;

	(void)mszGroups;
	PROFILE_START
	API_TRACE_IN("%ld", hContext)

	/*
	 * Check for NULL parameters
	 */
	if (pcchReaders == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Make sure this context has been opened
	 */
	currentContextMap = SCardGetAndLockContext(hContext);
	if (NULL == currentContextMap)
	{
		PROFILE_END(SCARD_E_INVALID_HANDLE)
		return SCARD_E_INVALID_HANDLE;
	}

	/* synchronize reader states with daemon */
	rv = getReaderStates(currentContextMap);
	if (rv != SCARD_S_SUCCESS)
		goto end;

	dwReadersLen = 0;
	for (i = 0; i < PCSCLITE_MAX_READERS_CONTEXTS; i++)
		if (readerStates[i].readerName[0] != '\0')
			dwReadersLen += strlen(readerStates[i].readerName) + 1;

	/* for the last NULL byte */
	dwReadersLen += 1;

	if (1 == dwReadersLen)
	{
		rv = SCARD_E_NO_READERS_AVAILABLE;
		goto end;
	}

	if (SCARD_AUTOALLOCATE == *pcchReaders)
	{
		if (NULL == mszReaders)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		buf = malloc(dwReadersLen);
		if (NULL == buf)
		{
			rv = SCARD_E_NO_MEMORY;
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
		if (readerStates[i].readerName[0] != '\0')
		{
			/*
			 * Build the multi-string
			 */
			strcpy(buf, readerStates[i].readerName);
			buf += strlen(readerStates[i].readerName)+1;
		}
	}
	*buf = '\0';	/* Add the last null */

end:
	/* set the reader names length */
	*pcchReaders = dwReadersLen;

	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)
	API_TRACE_OUT("%d", *pcchReaders)

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
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hContext handle (\ref SCARD_E_INVALID_HANDLE)
 */

LONG SCardFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	LONG rv = SCARD_S_SUCCESS;

	PROFILE_START

	/*
	 * Make sure this context has been opened
	 */
	if (! SCardGetContextValidity(hContext))
		return SCARD_E_INVALID_HANDLE;

	free((void *)pvMem);

	PROFILE_END(rv)

	return rv;
}

/**
 * @brief Returns a list of currently available reader groups on
 * the system. \p mszGroups is a pointer to a character string that is
 * allocated by the application.  If the application sends \p mszGroups as NULL
 * then this function will return the size of the buffer needed to allocate in
 * \p pcchGroups.
 *
 * The group names is a multi-string and separated by a null character (\c
 * '\0') and ended by a double null character like
 * \c "SCard$DefaultReaders\0Group 2\0\0".
 *
 * If \c *pcchGroups is equal to \ref SCARD_AUTOALLOCATE then the function
 * will allocate itself the needed memory. Use SCardFreeMemory() to release it.
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 * @param[out] mszGroups List of groups to list readers.
 * @param[in,out] pcchGroups Size of multi-string buffer including NULL's.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INSUFFICIENT_BUFFER Reader buffer not large enough (\ref SCARD_E_INSUFFICIENT_BUFFER)
 * @retval SCARD_E_INVALID_HANDLE Invalid Scope Handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_INVALID_PARAMETER \p mszGroups is NULL and \p *pcchGroups == \ref SCARD_AUTOALLOCATE (\ref SCARD_E_INVALID_PARAMETER)
 * @retval SCARD_E_NO_MEMORY Memory allocation failed (\ref SCARD_E_NO_MEMORY)
 * @retval SCARD_E_NO_SERVICE The server is not running (\ref SCARD_E_NO_SERVICE)
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
	SCONTEXTMAP * currentContextMap;
	char *buf = NULL;

	PROFILE_START

	/* Multi-string with two trailing \0 */
	const char ReaderGroup[] = "SCard$DefaultReaders\0";
	const unsigned int dwGroups = sizeof(ReaderGroup);

	/*
	 * Make sure this context has been opened
	 */
	currentContextMap = SCardGetAndLockContext(hContext);
	if (NULL == currentContextMap)
		return SCARD_E_INVALID_HANDLE;

	if (SCARD_AUTOALLOCATE == *pcchGroups)
	{
		if (NULL == mszGroups)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto end;
		}
		buf = malloc(dwGroups);
		if (NULL == buf)
		{
			rv = SCARD_E_NO_MEMORY;
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

	(void)pthread_mutex_unlock(&currentContextMap->mMutex);

	PROFILE_END(rv)

	return rv;
}

/**
 * Cancels a specific blocking \ref SCardGetStatusChange() function.
 * MUST be called with the same \ref SCARDCONTEXT as \ref
 * SCardGetStatusChange().
 *
 * @ingroup API
 * @param[in] hContext Connection context to the PC/SC Resource Manager.
 *
 * @return Error code.
 * @retval SCARD_S_SUCCESS Successful (\ref SCARD_S_SUCCESS)
 * @retval SCARD_E_INVALID_HANDLE Invalid \p hContext handle (\ref SCARD_E_INVALID_HANDLE)
 * @retval SCARD_E_NO_SERVICE Server is not running (\ref SCARD_E_NO_SERVICE)
 * @retval SCARD_F_COMM_ERROR An internal communications error has been detected (\ref SCARD_F_COMM_ERROR)
 *
 * @code
 * SCARDCONTEXT hContext;
 * DWORD cReaders;
 * SCARD_READERSTATE rgReaderStates;
 * LONG rv;
 * ...
 * rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
 * rgReaderStates.szReader = "Reader X";
 * rgReaderStates.dwCurrentState = SCARD_STATE_EMPTY;
 * cReaders = 1;
 * ...
 * rv = SCardGetStatusChange(hContext, INFINITE, &rgReaderStates, cReaders);
 * ...
 * / * Spawn off thread for following function * /
 * rv = SCardCancel(hContext);
 * @endcode
 */
LONG SCardCancel(SCARDCONTEXT hContext)
{
	SCONTEXTMAP * currentContextMap;
	LONG rv = SCARD_S_SUCCESS;
	uint32_t dwClientID = 0;
	struct cancel_struct scCancelStruct;
	char cancellable;

	PROFILE_START
	API_TRACE_IN("%ld", hContext)

	/*
	 * Make sure this context has been opened
	 */
	(void)SCardLockThread();
	currentContextMap = SCardGetContextTH(hContext);

	if (NULL == currentContextMap)
	{
		(void)SCardUnlockThread();
		rv = SCARD_E_INVALID_HANDLE;
		goto error;
	}
	cancellable = currentContextMap->cancellable;
	(void)SCardUnlockThread();

	if (! cancellable)
	{
		rv = SCARD_S_SUCCESS;
		goto error;
	}

	/* create a new connection to the server */
	if (ClientSetupSession(&dwClientID) != 0)
	{
		rv = SCARD_E_NO_SERVICE;
		goto error;
	}

	scCancelStruct.hContext = hContext;
	scCancelStruct.rv = SCARD_S_SUCCESS;

	rv = MessageSendWithHeader(SCARD_CANCEL, dwClientID,
		sizeof(scCancelStruct), (void *) &scCancelStruct);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	/*
	 * Read a message from the server
	 */
	rv = MessageReceive(&scCancelStruct, sizeof(scCancelStruct), dwClientID);

	if (rv != SCARD_S_SUCCESS)
		goto end;

	rv = scCancelStruct.rv;
end:
	ClientCloseSession(dwClientID);

error:
	PROFILE_END(rv)
	API_TRACE_OUT("")

	return rv;
}

/**
 * @brief Check if a \ref SCARDCONTEXT is valid.
 *
 * Call this function to determine whether a smart card context handle is still
 * valid. After a smart card context handle has been returned by
 * SCardEstablishContext(), it may become invalid if the resource manager
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

	PROFILE_START
	API_TRACE_IN("%ld", hContext)

	rv = SCARD_S_SUCCESS;

	/*
	 * Make sure this context has been opened
	 */
	if (! SCardGetContextValidity(hContext))
		rv = SCARD_E_INVALID_HANDLE;

	PROFILE_END(rv)
	API_TRACE_OUT("")

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
	int lrv;
	SCONTEXTMAP * newContextMap;

	newContextMap = malloc(sizeof(SCONTEXTMAP));
	if (NULL == newContextMap)
		return SCARD_E_NO_MEMORY;

	Log2(PCSC_LOG_DEBUG, "Allocating new SCONTEXTMAP @%p", newContextMap);
	newContextMap->hContext = hContext;
	newContextMap->dwClientID = dwClientID;
	newContextMap->cancellable = FALSE;

	(void)pthread_mutex_init(&newContextMap->mMutex, NULL);

	lrv = list_init(&newContextMap->channelMapList);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_init failed with return value: %d", lrv);
		goto error;
	}

	lrv = list_attributes_seeker(&newContextMap->channelMapList,
		CHANNEL_MAP_seeker);
	if (lrv <0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_attributes_seeker failed with return value: %d", lrv);
		list_destroy(&newContextMap->channelMapList);
		goto error;
	}

	lrv = list_append(&contextMapList, newContextMap);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL, "list_append failed with return value: %d",
			lrv);
		list_destroy(&newContextMap->channelMapList);
		goto error;
	}

	return SCARD_S_SUCCESS;

error:

	(void)pthread_mutex_destroy(&newContextMap->mMutex);
	free(newContextMap);

	return SCARD_E_NO_MEMORY;
}

/**
 * @brief Get the \ref SCONTEXTMAP * from the Application Context
 * vector \c _psContextMap for the passed context.
 *
 * This function is a thread-safe wrapper to the function
 * SCardGetContextTH().
 *
 * If the context is valid then \c &currentContextMap->mMutex lock is
 * acquired. The mutex lock needs to be released when the structure is
 * no more used.
 *
 * @param[in] hContext Application Context whose SCONTEXTMAP will be find.
 *
 * @return context map corresponding to the Application Context or NULL
 * if it is not found.
 */
static SCONTEXTMAP * SCardGetAndLockContext(SCARDCONTEXT hContext)
{
	SCONTEXTMAP * currentContextMap;

	SCardLockThread();
	currentContextMap = SCardGetContextTH(hContext);

	/* lock the context (if available) */
	if (NULL != currentContextMap)
		(void)pthread_mutex_lock(&currentContextMap->mMutex);

	SCardUnlockThread();

	return currentContextMap;
}

/**
 * @brief Get the address from the Application Context list \c _psContextMap
 * for the passed context.
 *
 * This functions is not thread-safe and should not be called. Instead, call
 * the function SCardGetContext().
 *
 * @param[in] hContext Application Context whose index will be find.
 *
 * @return Address corresponding to the Application Context or NULL if it is
 * not found.
 */
static SCONTEXTMAP * SCardGetContextTH(SCARDCONTEXT hContext)
{
	return list_seek(&contextMapList, &hContext);
}

/**
 * @brief Removes an Application Context from a control vector.
 *
 * @param[in] hContext Application Context to be removed.
 *
 */
static void SCardRemoveContext(SCARDCONTEXT hContext)
{
	SCONTEXTMAP * currentContextMap;
	currentContextMap = SCardGetContextTH(hContext);

	if (NULL != currentContextMap)
		SCardCleanContext(currentContextMap);
}

static void SCardCleanContext(SCONTEXTMAP * targetContextMap)
{
	int list_index, lrv;
	int listSize;
	CHANNEL_MAP * currentChannelMap;

	targetContextMap->hContext = 0;
	ClientCloseSession(targetContextMap->dwClientID);
	targetContextMap->dwClientID = 0;
	(void)pthread_mutex_destroy(&targetContextMap->mMutex);

	listSize = list_size(&targetContextMap->channelMapList);
	for (list_index = 0; list_index < listSize; list_index++)
	{
		currentChannelMap = list_get_at(&targetContextMap->channelMapList,
			list_index);
		if (NULL == currentChannelMap)
		{
			Log2(PCSC_LOG_CRITICAL, "list_get_at failed for index %d",
				list_index);
			continue;
		}
		else
		{
			free(currentChannelMap->readerName);
			free(currentChannelMap);
		}

	}
	list_destroy(&targetContextMap->channelMapList);

	lrv = list_delete(&contextMapList, targetContextMap);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_delete failed with return value: %d", lrv);
	}

	free(targetContextMap);

	return;
}

/*
 * Functions for managing hCard values returned from SCardConnect.
 */

static LONG SCardAddHandle(SCARDHANDLE hCard, SCONTEXTMAP * currentContextMap,
	LPCSTR readerName)
{
	CHANNEL_MAP * newChannelMap;
	int lrv = -1;

	newChannelMap = malloc(sizeof(CHANNEL_MAP));
	if (NULL == newChannelMap)
		return SCARD_E_NO_MEMORY;

	newChannelMap->hCard = hCard;
	newChannelMap->readerName = strdup(readerName);

	lrv = list_append(&currentContextMap->channelMapList, newChannelMap);
	if (lrv < 0)
	{
		free(newChannelMap->readerName);
		free(newChannelMap);
		Log2(PCSC_LOG_CRITICAL, "list_append failed with return value: %d",
			lrv);
		return SCARD_E_NO_MEMORY;
	}

	return SCARD_S_SUCCESS;
}

static void SCardRemoveHandle(SCARDHANDLE hCard)
{
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * currentChannelMap;
	int lrv;
	LONG rv;

	rv = SCardGetContextAndChannelFromHandleTH(hCard, &currentContextMap,
		&currentChannelMap);
	if (rv == -1)
		return;

	free(currentChannelMap->readerName);

	lrv = list_delete(&currentContextMap->channelMapList, currentChannelMap);
	if (lrv < 0)
	{
		Log2(PCSC_LOG_CRITICAL,
			"list_delete failed with return value: %d", lrv);
	}

	free(currentChannelMap);

	return;
}

static LONG SCardGetContextChannelAndLockFromHandle(SCARDHANDLE hCard,
	SCONTEXTMAP **targetContextMap, CHANNEL_MAP ** targetChannelMap)
{
	LONG rv;

	if (0 == hCard)
		return -1;

	SCardLockThread();
	rv = SCardGetContextAndChannelFromHandleTH(hCard, targetContextMap,
		targetChannelMap);

	if (SCARD_S_SUCCESS == rv)
		(void)pthread_mutex_lock(&(*targetContextMap)->mMutex);

	SCardUnlockThread();

	return rv;
}

static LONG SCardGetContextAndChannelFromHandleTH(SCARDHANDLE hCard,
	SCONTEXTMAP **targetContextMap, CHANNEL_MAP ** targetChannelMap)
{
	int listSize;
	int list_index;
	SCONTEXTMAP * currentContextMap;
	CHANNEL_MAP * currentChannelMap;

	/* Best to get the caller a crash early if we fail unsafely */
	*targetContextMap = NULL;
	*targetChannelMap = NULL;

	listSize = list_size(&contextMapList);

	for (list_index = 0; list_index < listSize; list_index++)
	{
		currentContextMap = list_get_at(&contextMapList, list_index);
		if (currentContextMap == NULL)
		{
			Log2(PCSC_LOG_CRITICAL, "list_get_at failed for index %d",
				list_index);
			continue;
		}
		currentChannelMap = list_seek(&currentContextMap->channelMapList,
			&hCard);
		if (currentChannelMap != NULL)
		{
			*targetContextMap = currentContextMap;
			*targetChannelMap = currentChannelMap;
			return SCARD_S_SUCCESS;
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
 */
LONG SCardCheckDaemonAvailability(void)
{
	LONG rv;
	struct stat statBuffer;
	char *socketName;

	socketName = getSocketName();
	rv = stat(socketName, &statBuffer);

	if (rv != 0)
	{
		Log3(PCSC_LOG_INFO, "PCSC Not Running: %s: %s",
			socketName, strerror(errno));
		return SCARD_E_NO_SERVICE;
	}

	return SCARD_S_SUCCESS;
}

static LONG getReaderStates(SCONTEXTMAP * currentContextMap)
{
	int32_t dwClientID = currentContextMap->dwClientID;
	LONG rv;

	rv = MessageSendWithHeader(CMD_GET_READERS_STATE, dwClientID, 0, NULL);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/* Read a message from the server */
	rv = MessageReceive(&readerStates, sizeof(readerStates), dwClientID);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	return SCARD_S_SUCCESS;
}

static LONG getReaderStatesAndRegisterForEvents(SCONTEXTMAP * currentContextMap)
{
	int32_t dwClientID = currentContextMap->dwClientID;
	LONG rv;

	/* Get current reader states from server and register on event list */
	rv = MessageSendWithHeader(CMD_WAIT_READER_STATE_CHANGE, dwClientID,
		0, NULL);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/* Read a message from the server */
	rv = MessageReceive(&readerStates, sizeof(readerStates), dwClientID);
	return rv;
}

static LONG unregisterFromEvents(SCONTEXTMAP * currentContextMap)
{
	int32_t dwClientID = currentContextMap->dwClientID;
	LONG rv;
	struct wait_reader_state_change waitStatusStruct = {0};

	/* ask server to remove us from the event list */
	rv = MessageSendWithHeader(CMD_STOP_WAITING_READER_STATE_CHANGE,
		dwClientID, 0, NULL);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/* This message can be the response to
	 * CMD_STOP_WAITING_READER_STATE_CHANGE, an event notification or a
	 * cancel notification.
	 * The server side ensures, that no more messages will be sent to
	 * the client. */

	rv = MessageReceive(&waitStatusStruct, sizeof(waitStatusStruct),
		dwClientID);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/* if we received a cancel event the return value will be set
	 * accordingly */
	rv = waitStatusStruct.rv;

	return rv;
}

