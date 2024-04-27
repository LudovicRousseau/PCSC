/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2004
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
 * @mainpage MUSCLE PC/SC-Lite API Documentation
 *
 * @section Introduction
 *
 * This document contains the reference API calls for communicating to the
 * MUSCLE PC/SC Smart Card Resource Manager. PC/SC is a standard proposed by
 * the PC/SC workgroup http://www.pcscworkgroup.com/ which is a conglomerate of
 * representative from major smart card manufacturers and other companies. This
 * specification tries to abstract the smart card layer into a high level API
 * so that smart cards and their readers can be accessed in a homogeneous
 * fashion.
 *
 * This toolkit was written in ANSI C that can be used with most compilers and
 * does NOT use complex and large data structures such as vectors, etc. The C
 * API emulates the winscard API that is used on the Windows platform. It is
 * contained in the library <tt>libpcsclite.so</tt> that is linked to your
 * application.
 *
 * I would really like to hear from you. If you have any feedback either on
 * this documentation or on the MUSCLE project please feel free to email me at:
 * corcoran@musclecard.com.
 *
 *
 * @section API_Routines
 *
 * These routines specified here are winscard.h routines like those in the
 * winscard API provided under Windows(R). These are compatible with the
 * Microsoft(R) API calls. This list of calls is mainly an abstraction of
 * readers. It gives a common API for communication to most readers in a
 * homogeneous fashion.
 *
 * Since all functions can produce a wide array of errors, please refer to
 * pcsclite.h for a list of error returns.
 *
 * For a human readable representation of an error the function
 * pcsc_stringify_error() is declared in pcsclite.h. This function is not
 * available on Microsoft(R) winscard API and is pcsc-lite specific.
 *
 * @section Internals
 *
 * PC/SC Lite is formed by a server daemon (<tt>pcscd</tt>) and a client
 * library (<tt>libpcsclite.so</tt>) that communicate via IPC.
 *
 * The file \em winscard_clnt.c in the client-side exposes the API for
 * applications.\n The file \em winscard.c has the server-side counterpart
 * functions present in \em winscard_clnt.c.\n The file \em winscard_msg.c is
 * the communication interface between \em winscard_clnt.c and \em
 * winscard.c.\n The file pcscdaemon.c has the main server-side function,
 * including a loop for accepting client requests.\n The file \em
 * winscard_svc.c has the functions called by \em pcscdaemon.c to serve clients
 * requests.
 *
 * When a function from \em winscard_clnt.c is called by a client application,
 * it calls a function in \em winscard_msg.c to send the message to \em
 * pcscdaemon.c.  When \em pcscdaemon.c a client detects a request arrived, it
 * calls \em winscard_svc.c which identifies what command the message contains
 * and requests \em winscard.c to execute the command.\n Meanwhile
 * winscard_clnt.c waits for the response until a timeout occurs.
 */

/**
 * @file
 * @brief This handles smart card reader communications.
 * This is the heart of the smart card API.
 *
 * Here are the main server-side functions which execute the requests from the
 * clients.
 */

#include "config.h"
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>

#include "pcscd.h"
#include "winscard.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "prothandler.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "sys_generic.h"
#include "eventhandler.h"
#include "utils.h"
#include "reader.h"

#undef DO_PROFILE
#ifdef DO_PROFILE

#define PROFILE_FILE "/tmp/pcscd_profile"
#include <stdio.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

struct timeval profile_time_start;
FILE *fd;
bool profile_tty;

#define PROFILE_START profile_start(__FUNCTION__);
#define PROFILE_END profile_end(__FUNCTION__, __LINE__);

static void profile_start(const char *f)
{
	static bool initialized = false;

	if (!initialized)
	{
		initialized = true;
		fd = fopen(PROFILE_FILE, "a+");
		if (NULL == fd)
		{
			fprintf(stderr, "\33[01;31mCan't open %s: %s\33[0m\n",
				PROFILE_FILE, strerror(errno));
			exit(-1);
		}
		fprintf(fd, "\nStart a new profile\n");
		fflush(fd);

		if (isatty(fileno(stderr)))
			profile_tty = true;
		else
			profile_tty = false;
	}

	gettimeofday(&profile_time_start, NULL);
} /* profile_start */


static void profile_end(const char *f, int line)
{
	struct timeval profile_time_end;
	long d;

	gettimeofday(&profile_time_end, NULL);
	d = time_sub(&profile_time_end, &profile_time_start);

	if (profile_tty)
		fprintf(stderr, "\33[01;31mRESULT %s \33[35m%ld\33[0m (%d)\n", f, d,
			line);
	fprintf(fd, "%s %ld\n", f, d);
	fflush(fd);
} /* profile_end */

#else
#define PROFILE_START
#define PROFILE_END
#endif

/** used for backward compatibility */
#define SCARD_PROTOCOL_ANY_OLD	 0x1000

static pthread_mutex_t LockMutex = PTHREAD_MUTEX_INITIALIZER;

LONG SCardEstablishContext(DWORD dwScope, /*@unused@*/ LPCVOID pvReserved1,
	/*@unused@*/ LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	(void)pvReserved1;
	(void)pvReserved2;

	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{
		*phContext = 0;
		return SCARD_E_INVALID_VALUE;
	}

	/*
	 * Unique identifier for this server so that it can uniquely be
	 * identified by clients and distinguished from others
	 */

	*phContext = SYS_RandomInt();

	Log2(PCSC_LOG_DEBUG, "Establishing Context: 0x%lX", *phContext);

	return SCARD_S_SUCCESS;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	/*
	 * Nothing to do here RPC layer will handle this
	 */
#ifdef NO_LOG
	(void)hContext;
#endif

	Log2(PCSC_LOG_DEBUG, "Releasing Context: 0x%lX", hContext);

	return SCARD_S_SUCCESS;
}

LONG SCardConnect(/*@unused@*/ SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	(void)hContext;
	PROFILE_START

	*phCard = 0;

	if ((dwShareMode != SCARD_SHARE_DIRECT) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	if (dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	Log3(PCSC_LOG_DEBUG, "Attempting Connect to %s using protocol: %ld",
		szReader, dwPreferredProtocols);

	rv = RFReaderInfo((LPSTR) szReader, &rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		Log2(PCSC_LOG_ERROR, "Reader %s Not Found", szReader);
		return rv;
	}

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*******************************************
	 *
	 * This section checks for simple errors
	 *
	 *******************************************/

	/*
	 * Connect if not exclusive mode
	 */
	if (rContext->contexts == PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
	{
		Log1(PCSC_LOG_ERROR, "Error Reader Exclusive");
		rv = SCARD_E_SHARING_VIOLATION;
		goto exit;
	}

	/*
	 * wait until a possible transaction is finished
	 */
	if (rContext->hLockId != 0)
	{
		Log1(PCSC_LOG_INFO, "Waiting for release of lock");
		while (rContext->hLockId != 0)
			(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		Log1(PCSC_LOG_INFO, "Lock released");
	}

	/*******************************************
	 *
	 * This section tries to determine the
	 * presence of a card or not
	 *
	 *******************************************/
	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		if (!(rContext->readerState->readerState & SCARD_PRESENT))
		{
			Log1(PCSC_LOG_DEBUG, "Card Not Inserted");
			rv = SCARD_E_NO_SMARTCARD;
			goto exit;
		}

		/* Power on (again) the card if needed */
		(void)pthread_mutex_lock(&rContext->powerState_lock);
		if (POWER_STATE_UNPOWERED == rContext->powerState)
		{
			DWORD dwAtrLen;

			dwAtrLen = sizeof(rContext->readerState->cardAtr);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr, &dwAtrLen);
			rContext->readerState->cardAtrLength = dwAtrLen;

			if (rv == IFD_SUCCESS)
			{
				rContext->readerState->readerState = SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;

				Log1(PCSC_LOG_DEBUG, "power up complete.");
				LogXxd(PCSC_LOG_DEBUG, "Card ATR: ",
					rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
			}
			else
				Log2(PCSC_LOG_ERROR, "Error powering up card: %s",
					rv2text(rv));
		}

		if (! (rContext->readerState->readerState & SCARD_POWERED))
		{
			Log1(PCSC_LOG_ERROR, "Card Not Powered");
			(void)pthread_mutex_unlock(&rContext->powerState_lock);
			rv = SCARD_W_UNPOWERED_CARD;
			goto exit;
		}

		/* the card is now in use */
		rContext->powerState = POWER_STATE_IN_USE;
		Log1(PCSC_LOG_DEBUG, "powerState: POWER_STATE_IN_USE");
		(void)pthread_mutex_unlock(&rContext->powerState_lock);
	}

	/*******************************************
	 *
	 * This section tries to decode the ATR
	 * and set up which protocol to use
	 *
	 *******************************************/
	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_RAW;
	else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			/* lock here instead in IFDSetPTS() to lock up to
			 * setting rContext->readerState->cardProtocol */
			(void)pthread_mutex_lock(rContext->mMutex);

			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNDEFINED == rContext->readerState->cardProtocol)
			{
				int availableProtocols, defaultProtocol;
				int ret;

				ATRDecodeAtr(&availableProtocols, &defaultProtocol,
					rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);

				/* If it is set to ANY let it do any of the protocols */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				/* restrict to the protocols requested by the user */
				availableProtocols &= dwPreferredProtocols;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					availableProtocols, defaultProtocol);

				/* keep cardProtocol = SCARD_PROTOCOL_UNDEFINED in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
				{
					(void)pthread_mutex_unlock(rContext->mMutex);
					rv = SCARD_W_UNRESPONSIVE_CARD;
					goto exit;
				}

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
				{
					(void)pthread_mutex_unlock(rContext->mMutex);
					rv = SCARD_E_PROTO_MISMATCH;
					goto exit;
				}

				/* use negotiated protocol */
				rContext->readerState->cardProtocol = ret;

				(void)pthread_mutex_unlock(rContext->mMutex);
			}
			else
			{
				(void)pthread_mutex_unlock(rContext->mMutex);

				if (! (dwPreferredProtocols & rContext->readerState->cardProtocol))
				{
					rv = SCARD_E_PROTO_MISMATCH;
					goto exit;
				}
			}
		}
	}

	*pdwActiveProtocol = rContext->readerState->cardProtocol;

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		switch (*pdwActiveProtocol)
		{
			case SCARD_PROTOCOL_T0:
			case SCARD_PROTOCOL_T1:
				Log2(PCSC_LOG_DEBUG, "Active Protocol: T=%d",
					(*pdwActiveProtocol == SCARD_PROTOCOL_T0) ? 0 : 1);
				break;

			case SCARD_PROTOCOL_RAW:
				Log1(PCSC_LOG_DEBUG, "Active Protocol: RAW");
				break;

			default:
				Log2(PCSC_LOG_ERROR, "Active Protocol: unknown %ld",
					*pdwActiveProtocol);
		}
	}
	else
		Log1(PCSC_LOG_DEBUG, "Direct access: no protocol selected");

	/*
	 * Prepare the SCARDHANDLE identity
	 */

	/* we need a lock to avoid concurrent generation of handles leading
	 * to a possible hCard handle duplication */
	(void)pthread_mutex_lock(&LockMutex);

	*phCard = RFCreateReaderHandle(rContext);

	Log2(PCSC_LOG_DEBUG, "hCard Identity: %lx", *phCard);

	/*******************************************
	 *
	 * This section tries to set up the
	 * exclusivity modes. -1 is exclusive
	 *
	 *******************************************/

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->contexts == PCSCLITE_SHARING_NO_CONTEXT)
		{
			rContext->contexts = PCSCLITE_SHARING_EXCLUSIVE_CONTEXT;
			(void)RFLockSharing(*phCard, rContext);
		}
		else
		{
			*phCard = 0;
			rv = SCARD_E_SHARING_VIOLATION;
			(void)pthread_mutex_unlock(&LockMutex);
			goto exit;
		}
	}
	else
	{
		/*
		 * Add a connection to the context stack
		 */
		rContext->contexts += 1;
	}

	/*
	 * Add this handle to the handle list
	 */
	rv = RFAddReaderHandle(rContext, *phCard);

	(void)pthread_mutex_unlock(&LockMutex);

	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Clean up - there is no more room
		 */
		if (rContext->contexts == PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
			rContext->contexts = PCSCLITE_SHARING_NO_CONTEXT;
		else
			if (rContext->contexts > PCSCLITE_SHARING_NO_CONTEXT)
				rContext->contexts -= 1;

		*phCard = 0;

		rv = SCARD_F_INTERNAL_ERROR;
		goto exit;
	}

	/*
	 * Propagate new state to reader state
	 */
	rContext->readerState->readerSharing = rContext->contexts;

exit:
	UNREF_READER(rContext)

	PROFILE_END

	return rv;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	Log1(PCSC_LOG_DEBUG, "Attempting reconnect to token.");

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Handle the dwInitialization
	 */
	if (dwInitialization != SCARD_LEAVE_CARD &&
			dwInitialization != SCARD_RESET_CARD &&
			dwInitialization != SCARD_UNPOWER_CARD)
		return SCARD_E_INVALID_VALUE;

	if (dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	if ((dwShareMode != SCARD_SHARE_DIRECT) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	if (dwInitialization == SCARD_RESET_CARD ||
		dwInitialization == SCARD_UNPOWER_CARD)
	{
		DWORD dwAtrLen;

		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		dwAtrLen = sizeof(rContext->readerState->cardAtr);
		if (SCARD_RESET_CARD == dwInitialization)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr, &dwAtrLen);
		else
		{
			IFDPowerICC(rContext, IFD_POWER_DOWN, NULL, NULL);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr, &dwAtrLen);
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

		/*
		 * Set up the status bit masks on readerState
		 */
		if (rv == IFD_SUCCESS)
		{
			rContext->readerState->cardAtrLength = dwAtrLen;
			rContext->readerState->readerState =
				SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;

			Log1(PCSC_LOG_DEBUG, "Reset complete.");
			LogXxd(PCSC_LOG_DEBUG, "Card ATR: ",
				rContext->readerState->cardAtr,
				rContext->readerState->cardAtrLength);
		}
		else
		{
			rContext->readerState->cardAtrLength = 0;
			Log1(PCSC_LOG_ERROR, "Error resetting card.");

			if (rv == SCARD_W_REMOVED_CARD)
			{
				rContext->readerState->readerState = SCARD_ABSENT;
				rv = SCARD_E_NO_SMARTCARD;
				goto exit;
			}
			else
			{
				rContext->readerState->readerState =
					SCARD_PRESENT | SCARD_SWALLOWED;
				rv = SCARD_W_UNRESPONSIVE_CARD;
				goto exit;
			}
		}
	}
	else
		if (dwInitialization == SCARD_LEAVE_CARD)
		{
			uint32_t readerState = rContext->readerState->readerState;

			if (readerState & SCARD_ABSENT)
			{
				rv = SCARD_E_NO_SMARTCARD;
				goto exit;
			}

			if ((readerState & SCARD_PRESENT)
				&& (readerState & SCARD_SWALLOWED))
			{
				rv = SCARD_W_UNRESPONSIVE_CARD;
				goto exit;
			}
		}

	/*******************************************
	 *
	 * This section tries to decode the ATR
	 * and set up which protocol to use
	 *
	 *******************************************/
	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_RAW;
	else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			/* lock here instead in IFDSetPTS() to lock up to
			 * setting rContext->readerState->cardProtocol */
			(void)pthread_mutex_lock(rContext->mMutex);

			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNDEFINED == rContext->readerState->cardProtocol)
			{
				int availableProtocols, defaultProtocol;
				int ret;

				ATRDecodeAtr(&availableProtocols, &defaultProtocol,
					rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);

				/* If it is set to ANY let it do any of the protocols */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				/* restrict to the protocols requested by the user */
				availableProtocols &= dwPreferredProtocols;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					availableProtocols, defaultProtocol);

				/* keep cardProtocol = SCARD_PROTOCOL_UNDEFINED in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
				{
					(void)pthread_mutex_unlock(rContext->mMutex);
					rv = SCARD_W_UNRESPONSIVE_CARD;
					goto exit;
				}

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
				{
					(void)pthread_mutex_unlock(rContext->mMutex);
					rv = SCARD_E_PROTO_MISMATCH;
					goto exit;
				}

				/* use negotiated protocol */
				rContext->readerState->cardProtocol = ret;

				(void)pthread_mutex_unlock(rContext->mMutex);
			}
			else
			{
				(void)pthread_mutex_unlock(rContext->mMutex);

				if (! (dwPreferredProtocols & rContext->readerState->cardProtocol))
				{
					rv = SCARD_E_PROTO_MISMATCH;
					goto exit;
				}
			}

			/* the card is now in use */
			RFSetPowerState(rContext, POWER_STATE_IN_USE);
			Log1(PCSC_LOG_DEBUG, "powerState: POWER_STATE_IN_USE");
		}
	}

	*pdwActiveProtocol = rContext->readerState->cardProtocol;

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		switch (*pdwActiveProtocol)
		{
			case SCARD_PROTOCOL_T0:
			case SCARD_PROTOCOL_T1:
				Log2(PCSC_LOG_DEBUG, "Active Protocol: T=%d",
					(*pdwActiveProtocol == SCARD_PROTOCOL_T0) ? 0 : 1);
				break;

			case SCARD_PROTOCOL_RAW:
				Log1(PCSC_LOG_DEBUG, "Active Protocol: RAW");
				break;

			default:
				Log2(PCSC_LOG_ERROR, "Active Protocol: unknown %ld",
					*pdwActiveProtocol);
		}
	}
	else
		Log1(PCSC_LOG_DEBUG, "Direct access: no protocol selected");

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->contexts == PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - we are already exclusive
			 */
		}
		else
		{
			if (rContext->contexts == PCSCLITE_SHARING_LAST_CONTEXT)
			{
				rContext->contexts = PCSCLITE_SHARING_EXCLUSIVE_CONTEXT;
				(void)RFLockSharing(hCard, rContext);
			}
			else
			{
				rv = SCARD_E_SHARING_VIOLATION;
				goto exit;
			}
		}
	}
	else if (dwShareMode == SCARD_SHARE_SHARED)
	{
		if (rContext->contexts != PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		}
		else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			(void)RFUnlockSharing(hCard, rContext);
			rContext->contexts = PCSCLITE_SHARING_LAST_CONTEXT;
		}
	}
	else if (dwShareMode == SCARD_SHARE_DIRECT)
	{
		if (rContext->contexts != PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		}
		else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			(void)RFUnlockSharing(hCard, rContext);
			rContext->contexts = PCSCLITE_SHARING_LAST_CONTEXT;
		}
	}
	else
	{
		rv = SCARD_E_INVALID_VALUE;
		goto exit;
	}

	/*
	 * Clear a previous event to the application
	 */
	(void)RFClearReaderEventState(rContext, hCard);

	/*
	 * Propagate new state to reader state
	 */
	rContext->readerState->readerSharing = rContext->contexts;

	rv = SCARD_S_SUCCESS;

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD))
		return SCARD_E_INVALID_VALUE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	/* ignore reader removal */
	if (SCARD_E_INVALID_VALUE == rv || SCARD_E_READER_UNAVAILABLE == rv)
		return SCARD_S_SUCCESS;
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * wait until a possible transaction is finished
	 */
	if ((dwDisposition != SCARD_LEAVE_CARD) && (rContext->hLockId != 0)
		&& (rContext->hLockId != hCard))
	{
		Log1(PCSC_LOG_INFO, "Waiting for release of lock");
		while (rContext->hLockId != 0)
			(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);
		Log1(PCSC_LOG_INFO, "Lock released");
	}

	/*
	 * Try to unlock any blocks on this context
	 *
	 * This may fail with SCARD_E_SHARING_VIOLATION if a transaction is
	 * on going on another card context and dwDisposition == SCARD_LEAVE_CARD.
	 * We should not stop.
	 */
	rv = RFUnlockAllSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		if (rv != SCARD_E_SHARING_VIOLATION)
		{
			goto exit;
		}
		else
		{
			if (SCARD_LEAVE_CARD != dwDisposition)
				goto exit;
		}
	}

	Log2(PCSC_LOG_DEBUG, "Active Contexts: %d", rContext->contexts);
	Log2(PCSC_LOG_DEBUG, "dwDisposition: %ld", dwDisposition);

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		DWORD dwAtrLen;

		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		dwAtrLen = sizeof(rContext->readerState->cardAtr);
		if (SCARD_RESET_CARD == dwDisposition)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr, &dwAtrLen);
		else
		{
			/* SCARD_UNPOWER_CARD */
			rv = IFDPowerICC(rContext, IFD_POWER_DOWN, NULL, NULL);

			RFSetPowerState(rContext, POWER_STATE_UNPOWERED);
			Log1(PCSC_LOG_DEBUG, "powerState: POWER_STATE_UNPOWERED");
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

		if (rv == IFD_SUCCESS)
		{
			if (SCARD_UNPOWER_CARD == dwDisposition)
				rContext->readerState->readerState = SCARD_PRESENT;
			else
			{
				rContext->readerState->cardAtrLength = dwAtrLen;
				rContext->readerState->readerState =
					SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;

				Log1(PCSC_LOG_DEBUG, "Reset complete.");
				LogXxd(PCSC_LOG_DEBUG, "Card ATR: ",
					rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
			}
		}
		else
		{
			if (SCARD_UNPOWER_CARD == dwDisposition)
				Log2(PCSC_LOG_ERROR, "Error powering down card: %s",
					rv2text(rv));
			else
			{
				rContext->readerState->cardAtrLength = 0;
				Log1(PCSC_LOG_ERROR, "Error resetting card.");
			}

			if (rv == SCARD_W_REMOVED_CARD)
				rContext->readerState->readerState = SCARD_ABSENT;
			else
				rContext->readerState->readerState =
					SCARD_PRESENT | SCARD_SWALLOWED;
		}
	}
	else if (dwDisposition == SCARD_EJECT_CARD)
	{
		UCHAR controlBuffer[5];
		UCHAR receiveBuffer[MAX_BUFFER_SIZE];
		DWORD receiveLength;

		/*
		 * Set up the CTBCS command for Eject ICC
		 */
		controlBuffer[0] = 0x20;
		controlBuffer[1] = 0x15;
		controlBuffer[2] = (rContext->slot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl_v2(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				Log1(PCSC_LOG_DEBUG, "Card ejected successfully.");
				/*
				 * Successful
				 */
			}
			else
				Log1(PCSC_LOG_ERROR, "Error ejecting card.");
		}
		else
			Log1(PCSC_LOG_ERROR, "Error ejecting card.");

	}
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	/*
	 * Remove and destroy this handle
	 */
	(void)RFRemoveReaderHandle(rContext, hCard);

	/*
	 * For exclusive connection reset it to no connections
	 */
	if (rContext->contexts == PCSCLITE_SHARING_EXCLUSIVE_CONTEXT)
		rContext->contexts = PCSCLITE_SHARING_NO_CONTEXT;
	else
	{
		/*
		 * Remove a connection from the context stack
		 */
		rContext->contexts -= 1;

		if (rContext->contexts < 0)
			rContext->contexts = 0;
	}

	if (PCSCLITE_SHARING_NO_CONTEXT == rContext->contexts)
	{
		RESPONSECODE (*fct)(DWORD) = NULL;
		DWORD dwGetSize;

		(void)pthread_mutex_lock(&rContext->powerState_lock);
		/* Switch to POWER_STATE_GRACE_PERIOD unless the card was not
		 * powered */
		if (POWER_STATE_POWERED <= rContext->powerState)
		{
			rContext->powerState = POWER_STATE_GRACE_PERIOD;
			Log1(PCSC_LOG_DEBUG, "powerState: POWER_STATE_GRACE_PERIOD");
		}

		(void)pthread_mutex_unlock(&rContext->powerState_lock);

		/* ask to stop the "polling" thread so it can be restarted using
		 * the correct timeout */
		dwGetSize = sizeof(fct);
		rv = IFDGetCapabilities(rContext, TAG_IFD_STOP_POLLING_THREAD,
			&dwGetSize, (PUCHAR)&fct);

		if ((IFD_SUCCESS == rv) && (dwGetSize == sizeof(fct)))
		{
			Log1(PCSC_LOG_INFO, "Stopping polling thread");
			fct(rContext->slot);
		}
	}

	/*
	 * Propagate new state to reader state
	 */
	rContext->readerState->readerSharing = rContext->contexts;

	rv = SCARD_S_SUCCESS;

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	READER_CONTEXT * rContext;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	rv = RFLockSharing(hCard, rContext);

	/* if the transaction is not yet ready we sleep a bit so the client
	 * do not retry immediately */
	if (SCARD_E_SHARING_VIOLATION == rv)
		(void)SYS_USleep(PCSCLITE_LOCK_POLL_RATE);

	Log2(PCSC_LOG_DEBUG, "Status: %s", rv2text(rv));

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	LONG rv2;
	READER_CONTEXT * rContext = NULL;

	/*
	 * Ignoring dwDisposition for now
	 */
	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD))
	return SCARD_E_INVALID_VALUE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Error if another transaction is ongoing and a card action is
	 * requested
	 */
	if ((dwDisposition != SCARD_LEAVE_CARD) && (rContext->hLockId != 0)
		&& (rContext->hLockId != hCard))
	{
		Log1(PCSC_LOG_INFO, "No card reset within a transaction");
		rv = SCARD_E_SHARING_VIOLATION;
		goto exit;
	}

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		DWORD dwAtrLen;

		dwAtrLen = sizeof(rContext->readerState->cardAtr);
		if (SCARD_RESET_CARD == dwDisposition)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr, &dwAtrLen);
		else
		{
			IFDPowerICC(rContext, IFD_POWER_DOWN, NULL, NULL);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr, &dwAtrLen);
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;

		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on readerState
		 */
		if (rv == IFD_SUCCESS)
		{
			rContext->readerState->cardAtrLength = dwAtrLen;
			rContext->readerState->readerState =
				SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;

			Log1(PCSC_LOG_DEBUG, "Reset complete.");
			LogXxd(PCSC_LOG_DEBUG, "Card ATR: ",
				rContext->readerState->cardAtr,
				rContext->readerState->cardAtrLength);
		}
		else
		{
			rContext->readerState->cardAtrLength = 0;
			Log1(PCSC_LOG_ERROR, "Error resetting card.");

			if (rv == SCARD_W_REMOVED_CARD)
				rContext->readerState->readerState = SCARD_ABSENT;
			else
				rContext->readerState->readerState =
					SCARD_PRESENT | SCARD_SWALLOWED;
		}
	}
	else if (dwDisposition == SCARD_EJECT_CARD)
	{
		UCHAR controlBuffer[5];
		UCHAR receiveBuffer[MAX_BUFFER_SIZE];
		DWORD receiveLength;

		/*
		 * Set up the CTBCS command for Eject ICC
		 */
		controlBuffer[0] = 0x20;
		controlBuffer[1] = 0x15;
		controlBuffer[2] = (rContext->slot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl_v2(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				Log1(PCSC_LOG_DEBUG, "Card ejected successfully.");
				/*
				 * Successful
				 */
			}
			else
				Log1(PCSC_LOG_ERROR, "Error ejecting card.");
		}
		else
			Log1(PCSC_LOG_ERROR, "Error ejecting card.");

	}
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	/*
	 * Unlock any blocks on this context
	 */
	/* we do not want to lose the previous rv value
	 * So we use another variable */
	rv2 = RFUnlockSharing(hCard, rContext);
	if (rv2 != SCARD_S_SUCCESS)
		/* if rv is already in error then do not change its value */
		if (rv == SCARD_S_SUCCESS)
			rv = rv2;

	Log2(PCSC_LOG_DEBUG, "Status: %s", rv2text(rv));

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR szReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	/* These parameters are not used by the client
	 * Client side code uses readerStates[] instead */
	(void)szReaderNames;
	(void)pcchReaderLen;
	(void)pdwState;
	(void)pdwProtocol;
	(void)pbAtr;
	(void)pcbAtrLen;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	if (rContext->readerState->cardAtrLength > MAX_ATR_SIZE)
	{
		rv = SCARD_F_INTERNAL_ERROR;
		goto exit;
	}

	/*
	 * This is a client side function however the server maintains the
	 * list of events between applications so it must be passed through to
	 * obtain this event if it has occurred
	 */

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode,
	LPCVOID pbSendBuffer, DWORD cbSendLength,
	LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	/* 0 bytes returned by default */
	*lpBytesReturned = 0;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	if (IFD_HVERSION_2_0 == rContext->version)
		if (NULL == pbSendBuffer || 0 == cbSendLength)
		{
			rv = SCARD_E_INVALID_PARAMETER;
			goto exit;
		}

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	if (IFD_HVERSION_2_0 == rContext->version)
	{
		/* we must wrap a API 3.0 client in an API 2.0 driver */
		*lpBytesReturned = cbRecvLength;
		rv = IFDControl_v2(rContext, (PUCHAR)pbSendBuffer,
			cbSendLength, pbRecvBuffer, lpBytesReturned);
	}
	else
		if (IFD_HVERSION_3_0 == rContext->version)
			rv = IFDControl(rContext, dwControlCode, pbSendBuffer,
				cbSendLength, pbRecvBuffer, cbRecvLength, lpBytesReturned);
		else
			rv = SCARD_E_UNSUPPORTED_FEATURE;

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	rv = IFDGetCapabilities(rContext, dwAttrId, pcbAttrLen, pbAttr);
	switch(rv)
	{
		case IFD_SUCCESS:
			rv = SCARD_S_SUCCESS;
			break;
		case IFD_ERROR_TAG:
			/* Special case SCARD_ATTR_DEVICE_FRIENDLY_NAME as it is better
			 * implemented in pcscd (it knows the friendly name)
			 */
			if ((SCARD_ATTR_DEVICE_FRIENDLY_NAME == dwAttrId)
				|| (SCARD_ATTR_DEVICE_SYSTEM_NAME == dwAttrId))
			{
				unsigned int len = strlen(rContext->readerState->readerName)+1;

				if (len > *pcbAttrLen)
					rv = SCARD_E_INSUFFICIENT_BUFFER;
				else
				{
					strcpy((char *)pbAttr, rContext->readerState->readerName);
					rv = SCARD_S_SUCCESS;
				}
				*pcbAttrLen = len;
			}
			else
				rv = SCARD_E_UNSUPPORTED_FEATURE;
			break;
		case IFD_ERROR_INSUFFICIENT_BUFFER:
			rv = SCARD_E_INSUFFICIENT_BUFFER;
			break;
		default:
			rv = SCARD_E_NOT_TRANSACTED;
	}

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	rv = IFDSetCapabilities(rContext, dwAttrId, cbAttrLen, (PUCHAR)pbAttr);
	if (rv == IFD_SUCCESS)
		rv = SCARD_S_SUCCESS;
	else
		if (rv == IFD_ERROR_TAG)
			rv = SCARD_E_UNSUPPORTED_FEATURE;
		else
			rv = SCARD_E_NOT_TRANSACTED;

exit:
	UNREF_READER(rContext)

	return rv;
}

LONG SCardTransmit(SCARDHANDLE hCard, const SCARD_IO_REQUEST *pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	SCARD_IO_REQUEST *pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	READER_CONTEXT * rContext = NULL;
	SCARD_IO_HEADER sSendPci, sRecvPci;
	DWORD dwRxLength, tempRxLength;

	dwRxLength = *pcbRecvLength;
	*pcbRecvLength = 0;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Must at least have 2 status words even for SCardControl
	 */
	if (dwRxLength < 2)
		return SCARD_E_INSUFFICIENT_BUFFER;

	/* get rContext corresponding to hCard */
	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	rv = RFCheckSharing(hCard, rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Make sure some event has not occurred
	 */
	rv = RFCheckReaderEventState(rContext, hCard);
	if (rv != SCARD_S_SUCCESS)
		goto exit;

	/*
	 * Check for some common errors
	 */
	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (rContext->readerState->readerState & SCARD_ABSENT)
		{
			rv = SCARD_E_NO_SMARTCARD;
			goto exit;
		}
	}

	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (pioSendPci->dwProtocol != SCARD_PROTOCOL_ANY_OLD)
		{
			if (pioSendPci->dwProtocol != rContext->readerState->cardProtocol)
			{
				rv = SCARD_E_PROTO_MISMATCH;
				goto exit;
			}
		}
	}

	/*
	 * Quick fix: PC/SC starts at 1 for bit masking but the IFD_Handler
	 * just wants 0 or 1
	 */

	sSendPci.Protocol = 0; /* protocol T=0 by default */

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_T1)
	{
		sSendPci.Protocol = 1;
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
	{
		/*
		 * This is temporary ......
		 */
		sSendPci.Protocol = SCARD_PROTOCOL_RAW;
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_ANY_OLD)
	{
	  /* Fix by Amira (Athena) */
		unsigned long i;
		unsigned long prot = rContext->readerState->cardProtocol;

		for (i = 0 ; prot != 1 && i < 16; i++)
			prot >>= 1;

		sSendPci.Protocol = i;
	}

	sSendPci.Length = pioSendPci->cbPciLength;

	sRecvPci.Protocol = pioRecvPci->dwProtocol;
	sRecvPci.Length = pioRecvPci->cbPciLength;

	/* the protocol number is decoded a few lines above */
	Log2(PCSC_LOG_DEBUG, "Send Protocol: T=%ld", sSendPci.Protocol);

	tempRxLength = dwRxLength;

	if ((pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
		&& (rContext->version == IFD_HVERSION_2_0))
	{
		rv = IFDControl_v2(rContext, (PUCHAR) pbSendBuffer, cbSendLength,
			pbRecvBuffer, &dwRxLength);
	} else
	{
		rv = IFDTransmit(rContext, sSendPci, (PUCHAR) pbSendBuffer,
			cbSendLength, pbRecvBuffer, &dwRxLength, &sRecvPci);
	}

	pioRecvPci->dwProtocol = sRecvPci.Protocol;
	pioRecvPci->cbPciLength = sRecvPci.Length;

	/*
	 * Check for any errors that might have occurred
	 */

	if (rv != SCARD_S_SUCCESS)
	{
		*pcbRecvLength = 0;
		Log2(PCSC_LOG_ERROR, "Card not transacted: %s", rv2text(rv));

        if (SCARD_E_NO_SMARTCARD == rv)
        {
            rContext->readerState->cardAtrLength = 0;
            rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNDEFINED;
            rContext->readerState->readerState = SCARD_ABSENT;
        }

		goto exit;
	}

	/*
	 * Available is less than received
	 */
	if (tempRxLength < dwRxLength)
	{
		*pcbRecvLength = 0;
		rv = SCARD_E_INSUFFICIENT_BUFFER;
		goto exit;
	}

	/*
	 * Successful return
	 */
	*pcbRecvLength = dwRxLength;

exit:
	UNREF_READER(rContext)

	return rv;
}

