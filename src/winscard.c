/*
 * This handles smartcard reader communications.
 * This is the heart of the M$ smartcard API.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

#include "pcsclite.h"
#include "winscard.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "prothandler.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "configfile.h"
#include "sys_generic.h"
#include "eventhandler.h"

#define SCARD_PROTOCOL_ANY_OLD	0x1000  /* used for backward compatibility */

/*
 * Some defines for context stack
 */
#define SCARD_LAST_CONTEXT       1
#define SCARD_NO_CONTEXT         0
#define SCARD_EXCLUSIVE_CONTEXT -1
#define SCARD_NO_LOCK            0

SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };
SCARD_IO_REQUEST g_rgSCardRawPci = { SCARD_PROTOCOL_RAW, 8 };

LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	/*
	 * Check for NULL pointer
	 */
	if (phContext == 0)
		return SCARD_E_INVALID_PARAMETER;

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

	*phContext = (PCSCLITE_SVC_IDENTITY + SYS_Random(SYS_GetSeed(),
			1.0, 65535.0));

	DebugLogB("Establishing Context: %d", *phContext);

	return SCARD_S_SUCCESS;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	/*
	 * Nothing to do here RPC layer will handle this
	 */

	DebugLogB("Releasing Context: %d", hContext);

	return SCARD_S_SUCCESS;
}

LONG SCardSetTimeout(SCARDCONTEXT hContext, DWORD dwTimeout)
{
	/*
	 * This is only used at the client side of an RPC call but just in
	 * case someone calls it here
	 */

	return SCARD_E_UNSUPPORTED_FEATURE;
}

LONG SCardConnect(SCARDCONTEXT hContext, LPCTSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	DWORD dwStatus;

	/*
	 * Check for NULL parameters
	 */
	if (szReader == 0 || phCard == 0 || pdwActiveProtocol == 0)
		return SCARD_E_INVALID_PARAMETER;
	else
		*phCard = 0;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	if (dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	DebugLogC("Attempting Connect to %s %d", szReader, dwPreferredProtocols);

	rv = RFReaderInfo((LPTSTR) szReader, &rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		DebugLogB("Reader %s Not Found", szReader);
		return rv;
	}

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*******************************************
	 *
	 * This section checks for simple errors
	 *
	 *******************************************/

	/*
	 * Connect if not exclusive mode
	 */
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
	{
		DebugLogA("Error Reader Exclusive");
		return SCARD_E_SHARING_VIOLATION;
	}

	/*******************************************
	 *
	 * This section tries to determine the
	 * presence of a card or not
	 *
	 *******************************************/
	dwStatus = rContext->readerState->readerState;

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		if (!(dwStatus & SCARD_PRESENT))
		{
			DebugLogA("Card Not Inserted");
			return SCARD_E_NO_SMARTCARD;
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
			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNSET == rContext->readerState->cardProtocol)
			{
				UCHAR ucAvailable, ucDefault;
				int ret;

				ucDefault = PHGetDefaultProtocol(rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
				ucAvailable =
					PHGetAvailableProtocols(rContext->readerState->cardAtr,
							rContext->readerState->cardAtrLength);

				/*
				 * If it is set to ANY let it do any of the protocols
				 */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable, ucDefault);

				/* keep cardProtocol = SCARD_PROTOCOL_UNSET in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
					return SCARD_W_UNRESPONSIVE_CARD;

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
					return SCARD_E_PROTO_MISMATCH;

				/* use negociated protocol */
				rContext->readerState->cardProtocol = ret;
			}
			else
			{
				if (! (dwPreferredProtocols & rContext->readerState->cardProtocol))
					return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	*pdwActiveProtocol = rContext->readerState->cardProtocol;

	if ((*pdwActiveProtocol != SCARD_PROTOCOL_T0)
		&& (*pdwActiveProtocol != SCARD_PROTOCOL_T1))
		DebugLogB("Active Protocol: unknown %d", *pdwActiveProtocol);
	else
		DebugLogB("Active Protocol: T=%d",
			(*pdwActiveProtocol == SCARD_PROTOCOL_T0) ? 0 : 1);

	/*
	 * Prepare the SCARDHANDLE identity
	 */
	*phCard = RFCreateReaderHandle(rContext);

	DebugLogB("hCard Identity: %x", *phCard);

	/*******************************************
	 *
	 * This section tries to set up the
	 * exclusivity modes. -1 is exclusive
	 *
	 *******************************************/

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->dwContexts == SCARD_NO_CONTEXT)
		{
			rContext->dwContexts = SCARD_EXCLUSIVE_CONTEXT;
			RFLockSharing(*phCard);
		}
		else
		{
			RFDestroyReaderHandle(*phCard);
			*phCard = 0;
			return SCARD_E_SHARING_VIOLATION;
		}
	}
	else
	{
		/*
		 * Add a connection to the context stack
		 */
		rContext->dwContexts += 1;
	}

	/*
	 * Add this handle to the handle list
	 */
	rv = RFAddReaderHandle(rContext, *phCard);

	if (rv != SCARD_S_SUCCESS)
	{
		/*
		 * Clean up - there is no more room
		 */
		RFDestroyReaderHandle(*phCard);
		if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
			rContext->dwContexts = SCARD_NO_CONTEXT;
		else
			if (rContext->dwContexts > SCARD_NO_CONTEXT)
				rContext->dwContexts -= 1;

		*phCard = 0;
		return SCARD_F_INTERNAL_ERROR;
	}

	/*
	 * Allow the status thread to convey information
	 */
	SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	return SCARD_S_SUCCESS;
}

LONG SCardReconnect(SCARDHANDLE hCard, DWORD dwShareMode,
	DWORD dwPreferredProtocols, DWORD dwInitialization,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	DebugLogA("Attempting reconnect to token.");

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if (dwInitialization != SCARD_LEAVE_CARD &&
			dwInitialization != SCARD_RESET_CARD &&
			dwInitialization != SCARD_UNPOWER_CARD)
		return SCARD_E_INVALID_VALUE;

	if (dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_DIRECT)
		return SCARD_E_INVALID_VALUE;

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD))
		return SCARD_E_PROTO_MISMATCH;

	if (pdwActiveProtocol == 0)
		return SCARD_E_INVALID_PARAMETER;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Handle the dwInitialization
	 */
	if ((dwInitialization != SCARD_LEAVE_CARD)
		&& (dwInitialization != SCARD_UNPOWER_CARD)
		&& (dwInitialization != SCARD_RESET_CARD))
		return SCARD_E_INVALID_VALUE;

	/*
	 * RFUnblockReader( rContext ); FIX - this doesn't work
	 */

	if (dwInitialization == SCARD_RESET_CARD ||
		dwInitialization == SCARD_UNPOWER_CARD)
	{
		/*
		 * Currently pcsc-lite keeps the card powered constantly
		 */
		if (SCARD_RESET_CARD == dwInitialization)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		else
		{
			rv = IFDPowerICC(rContext, IFD_POWER_DOWN,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		/*
		 * Notify the card has been reset
		 * Not doing this could result in deadlock
		 */
		rv = RFCheckReaderEventState(rContext, hCard);
		switch(rv)
		{
			/* avoid deadlock */
			case SCARD_W_RESET_CARD:
				break;

			case SCARD_W_REMOVED_CARD:
				DebugLogA("card removed");
				return SCARD_W_REMOVED_CARD;

			/* invalid EventStatus */
			case SCARD_E_INVALID_VALUE:
				DebugLogA("invalid EventStatus");
				return SCARD_F_INTERNAL_ERROR;

			/* invalid hCard, but hCard was widely used some lines above :( */
			case SCARD_E_INVALID_HANDLE:
				DebugLogA("invalid handle");
				return SCARD_F_INTERNAL_ERROR;

			case SCARD_S_SUCCESS:
				/*
				 * Notify the card has been reset
				 */
				RFSetReaderEventState(rContext, SCARD_RESET);

				/*
				 * Set up the status bit masks on dwStatus
				 */
				if (rv == SCARD_S_SUCCESS)
				{
					rContext->readerState->readerState |= SCARD_PRESENT;
					rContext->readerState->readerState &= ~SCARD_ABSENT;
					rContext->readerState->readerState |= SCARD_POWERED;
					rContext->readerState->readerState |= SCARD_NEGOTIABLE;
					rContext->readerState->readerState &= ~SCARD_SPECIFIC;
					rContext->readerState->readerState &= ~SCARD_SWALLOWED;
					rContext->readerState->readerState &= ~SCARD_UNKNOWN;
				}
				else
				{
					rContext->readerState->readerState |= SCARD_PRESENT;
					rContext->readerState->readerState &= ~SCARD_ABSENT;
					rContext->readerState->readerState |= SCARD_SWALLOWED;
					rContext->readerState->readerState &= ~SCARD_POWERED;
					rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
					rContext->readerState->readerState &= ~SCARD_SPECIFIC;
					rContext->readerState->readerState &= ~SCARD_UNKNOWN;
					rContext->readerState->cardAtrLength = 0;
				}

				if (rContext->readerState->cardAtrLength > 0)
				{
					DebugLogA("Reset complete.");
					DebugXxd("Card ATR: ", rContext->readerState->cardAtr,
						rContext->readerState->cardAtrLength);
				}
				else
				{
					DWORD dwStatus, dwAtrLen;
					UCHAR ucAtr[MAX_ATR_SIZE];

					DebugLogA("Error resetting card.");
					IFDStatusICC(rContext, &dwStatus, ucAtr, &dwAtrLen);
					if (dwStatus & SCARD_PRESENT)
						return SCARD_W_UNRESPONSIVE_CARD;
					else
						return SCARD_E_NO_SMARTCARD;
				}
				break;

			default:
				DebugLogB("invalid retcode from RFCheckReaderEventState (%X)", rv);
				return SCARD_F_INTERNAL_ERROR;
				break;
		}

	}
	else
		if (dwInitialization == SCARD_LEAVE_CARD)
		{
			/*
			 * Do nothing
			 */
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
			/* the protocol is not yet set (no PPS yet) */
			if (SCARD_PROTOCOL_UNSET == rContext->readerState->cardProtocol)
			{
				UCHAR ucAvailable, ucDefault;
				int ret;

				ucDefault = PHGetDefaultProtocol(rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
				ucAvailable =
					PHGetAvailableProtocols(rContext->readerState->cardAtr,
							rContext->readerState->cardAtrLength);

				/* If it is set to ANY let it do any of the protocols */
				if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)
					dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;

				ret = PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable, ucDefault);

				/* keep cardProtocol = SCARD_PROTOCOL_UNSET in case of error  */
				if (SET_PROTOCOL_PPS_FAILED == ret)
					return SCARD_W_UNRESPONSIVE_CARD;

				if (SET_PROTOCOL_WRONG_ARGUMENT == ret)
					return SCARD_E_PROTO_MISMATCH;

				/* use negociated protocol */
				rContext->readerState->cardProtocol = ret;
			}
			else
			{
				if (! (dwPreferredProtocols & rContext->readerState->cardProtocol))
					return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	*pdwActiveProtocol = rContext->readerState->cardProtocol;

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - we are already exclusive
			 */
		} else
		{
			if (rContext->dwContexts == SCARD_LAST_CONTEXT)
			{
				rContext->dwContexts = SCARD_EXCLUSIVE_CONTEXT;
				RFLockSharing(hCard);
			} else
			{
				return SCARD_E_SHARING_VIOLATION;
			}
		}
	} else if (dwShareMode == SCARD_SHARE_SHARED)
	{
		if (rContext->dwContexts != SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		} else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			RFUnlockSharing(hCard);
			rContext->dwContexts = SCARD_LAST_CONTEXT;
		}
	} else if (dwShareMode == SCARD_SHARE_DIRECT)
	{
		if (rContext->dwContexts != SCARD_EXCLUSIVE_CONTEXT)
		{
			/*
			 * Do nothing - in sharing mode already
			 */
		} else
		{
			/*
			 * We are in exclusive mode but want to share now
			 */
			RFUnlockSharing(hCard);
			rContext->dwContexts = SCARD_LAST_CONTEXT;
		}
	} else
		return SCARD_E_INVALID_VALUE;

	/*
	 * Clear a previous event to the application
	 */
	RFClearReaderEventState(rContext, hCard);

	/*
	 * Allow the status thread to convey information
	 */
	SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	return SCARD_S_SUCCESS;
}

LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD))
		return SCARD_E_INVALID_VALUE;

	/*
	 * Unlock any blocks on this context
	 */
	RFUnlockSharing(hCard);

	DebugLogB("Active Contexts: %d", rContext->dwContexts);

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		/*
		 * Currently pcsc-lite keeps the card powered constantly
		 */
		if (SCARD_RESET_CARD == dwDisposition)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		else
		{
			rv = IFDPowerICC(rContext, IFD_POWER_DOWN,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on dwStatus
		 */
		if (rv == SCARD_S_SUCCESS)
		{
			rContext->readerState->readerState |= SCARD_PRESENT;
			rContext->readerState->readerState &= ~SCARD_ABSENT;
			rContext->readerState->readerState |= SCARD_POWERED;
			rContext->readerState->readerState |= SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~SCARD_SPECIFIC;
			rContext->readerState->readerState &= ~SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~SCARD_UNKNOWN;
		}
		else
		{
			if (rContext->readerState->readerState & SCARD_ABSENT)
				rContext->readerState->readerState &= ~SCARD_PRESENT;
			else
				rContext->readerState->readerState |= SCARD_PRESENT;
			/* SCARD_ABSENT flag is already set */
			rContext->readerState->readerState |= SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~SCARD_POWERED;
			rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~SCARD_SPECIFIC;
			rContext->readerState->readerState &= ~SCARD_UNKNOWN;
			rContext->readerState->cardAtrLength = 0;
		}

		if (rContext->readerState->cardAtrLength > 0)
			DebugLogA("Reset complete.");
		else
			DebugLogA("Error resetting card.");

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
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl_v2(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				DebugLogA("Card ejected successfully.");
				/*
				 * Successful
				 */
			}
			else
				DebugLogA("Error ejecting card.");
		}
		else
			DebugLogA("Error ejecting card.");

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
	RFRemoveReaderHandle(rContext, hCard);
	RFDestroyReaderHandle(hCard);

	/*
	 * For exclusive connection reset it to no connections
	 */
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
	{
		rContext->dwContexts = SCARD_NO_CONTEXT;
		return SCARD_S_SUCCESS;
	}

	/*
	 * Remove a connection from the context stack
	 */
	rContext->dwContexts -= 1;

	if (rContext->dwContexts < 0)
		rContext->dwContexts = 0;

	/*
	 * Allow the status thread to convey information
	 */
	SYS_USleep(PCSCLITE_STATUS_POLL_RATE + 10);

	return SCARD_S_SUCCESS;
}

LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFLockSharing(hCard);

	DebugLogB("Status: %d.", rv);

	return rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

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

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{
		/*
		 * Currently pcsc-lite keeps the card always powered
		 */
		if (SCARD_RESET_CARD == dwDisposition)
			rv = IFDPowerICC(rContext, IFD_RESET,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		else
		{
			rv = IFDPowerICC(rContext, IFD_POWER_DOWN,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
			rv = IFDPowerICC(rContext, IFD_POWER_UP,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);
		}

		/* the protocol is unset after a power on */
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		/*
		 * Notify the card has been reset
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on dwStatus
		 */
		if (rv == SCARD_S_SUCCESS)
		{
			rContext->readerState->readerState |= SCARD_PRESENT;
			rContext->readerState->readerState &= ~SCARD_ABSENT;
			rContext->readerState->readerState |= SCARD_POWERED;
			rContext->readerState->readerState |= SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~SCARD_SPECIFIC;
			rContext->readerState->readerState &= ~SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~SCARD_UNKNOWN;
		}
		else
		{
			if (rContext->readerState->readerState & SCARD_ABSENT)
				rContext->readerState->readerState &= ~SCARD_PRESENT;
			else
				rContext->readerState->readerState |= SCARD_PRESENT;
			/* SCARD_ABSENT flag is already set */
			rContext->readerState->readerState |= SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~SCARD_POWERED;
			rContext->readerState->readerState &= ~SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~SCARD_SPECIFIC;
			rContext->readerState->readerState &= ~SCARD_UNKNOWN;
			rContext->readerState->cardAtrLength = 0;
		}

		if (rContext->readerState->cardAtrLength > 0)
			DebugLogA("Reset complete.");
		else
			DebugLogA("Error resetting card.");

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
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl_v2(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				DebugLogA("Card ejected successfully.");
				/*
				 * Successful
				 */
			}
			else
				DebugLogA("Error ejecting card.");
		}
		else
			DebugLogA("Error ejecting card.");

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
	RFUnlockSharing(hCard);

	DebugLogB("Status: %d.", rv);

	return rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	/*
	 * Ignoring dwDisposition for now
	 */
	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFUnlockSharing(hCard);

	DebugLogB("Status: %d.", rv);

	return rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPTSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context
	 */
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (strlen(rContext->lpcReader) > MAX_BUFFER_SIZE
			|| rContext->readerState->cardAtrLength > MAX_ATR_SIZE
			|| rContext->readerState->cardAtrLength < 0)
		return SCARD_F_INTERNAL_ERROR;

	/*
	 * This is a client side function however the server maintains the
	 * list of events between applications so it must be passed through to
	 * obtain this event if it has occurred
	 */

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (mszReaderNames)
	{  /* want reader name */
		if (pcchReaderLen)
		{ /* & present reader name length */
			if (*pcchReaderLen >= strlen(rContext->lpcReader))
			{ /* & enough room */
				*pcchReaderLen = strlen(rContext->lpcReader);
				strncpy(mszReaderNames, rContext->lpcReader, MAX_READERNAME);
			}
			else
			{        /* may report only reader name len */
				*pcchReaderLen = strlen(rContext->lpcReader);
				rv = SCARD_E_INSUFFICIENT_BUFFER;
			}
		}
		else
		{            /* present buf & no buflen */
			return SCARD_E_INVALID_PARAMETER;
		}
	}
	else
	{
		if (pcchReaderLen)
		{ /* want reader len only */
			*pcchReaderLen = strlen(rContext->lpcReader);
		}
		else
		{
		/* nothing todo */
		}
	}

	if (pdwState)
		*pdwState = rContext->readerState->readerState;

	if (pdwProtocol)
		*pdwProtocol = rContext->readerState->cardProtocol;

	if (pbAtr)
	{  /* want ATR */
		if (pcbAtrLen)
		{ /* & present ATR length */
			if (*pcbAtrLen >= rContext->readerState->cardAtrLength)
			{ /* & enough room */
				*pcbAtrLen = rContext->readerState->cardAtrLength;
				memcpy(pbAtr, rContext->readerState->cardAtr,
					rContext->readerState->cardAtrLength);
			}
			else
			{ /* may report only ATR len */
				*pcbAtrLen = rContext->readerState->cardAtrLength;
				rv = SCARD_E_INSUFFICIENT_BUFFER;
			}
		}
		else
		{ /* present buf & no buflen */
			return SCARD_E_INVALID_PARAMETER;
		}
	}
	else
	{
		if (pcbAtrLen)
		{ /* want ATR len only */
			*pcbAtrLen = rContext->readerState->cardAtrLength;
		}
		else
		{
			/* nothing todo */
		}
	}

	return rv;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE hCard, DWORD dwControlCode,
	LPCVOID pbSendBuffer, DWORD cbSendLength,
	LPVOID pbRecvBuffer, DWORD cbRecvLength, LPDWORD lpBytesReturned)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	/* 0 bytes returned by default */
	*lpBytesReturned = 0;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	if (NULL == pbSendBuffer || 0 == cbSendLength)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	if (cbSendLength > MAX_BUFFER_SIZE)
		return SCARD_E_INSUFFICIENT_BUFFER;

	if (IFD_HVERSION_2_0 == rContext->dwVersion)
	{
		/* we must wrap a API 3.0 client in an API 2.0 driver */
		*lpBytesReturned = cbRecvLength;
		return IFDControl_v2(rContext, (PUCHAR)pbSendBuffer,
			cbSendLength, pbRecvBuffer, lpBytesReturned);
	}
	else
		if (IFD_HVERSION_3_0 == rContext->dwVersion)
			return IFDControl(rContext, dwControlCode, pbSendBuffer,
				cbSendLength, pbRecvBuffer, cbRecvLength, lpBytesReturned);
		else
			return SCARD_E_UNSUPPORTED_FEATURE;
}

LONG SCardGetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPBYTE pbAttr, LPDWORD pcbAttrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = IFDGetCapabilities(rContext, dwAttrId, pcbAttrLen, pbAttr);
	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_NOT_TRANSACTED;
}

LONG SCardSetAttrib(SCARDHANDLE hCard, DWORD dwAttrId,
	LPCBYTE pbAttr, DWORD cbAttrLen)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	if (0 == hCard)
		return SCARD_E_INVALID_HANDLE;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = IFDSetCapabilities(rContext, dwAttrId, cbAttrLen, (PUCHAR)pbAttr);
	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
		return SCARD_E_NOT_TRANSACTED;
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	SCARD_IO_HEADER sSendPci, sRecvPci;
	DWORD dwRxLength, tempRxLength;

	if (pcbRecvLength == 0)
		return SCARD_E_INVALID_PARAMETER;

	dwRxLength = *pcbRecvLength;
	*pcbRecvLength = 0;

	if (hCard == 0)
		return SCARD_E_INVALID_HANDLE;

	if (pbSendBuffer == NULL || pbRecvBuffer == NULL || pioSendPci == NULL)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Must at least send a 4 bytes APDU
	 */
	if (cbSendLength < 4)
		return SCARD_E_INVALID_PARAMETER;

	/*
	 * Must at least have 2 status words even for SCardControl
	 */
	if (dwRxLength < 2)
		return SCARD_E_INSUFFICIENT_BUFFER;

	/*
	 * Make sure no one has a lock on this reader
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
		return rv;

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure the reader is working properly
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Make sure some event has not occurred
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Check for some common errors
	 */
	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (rContext->readerState->readerState & SCARD_ABSENT)
		{
			return SCARD_E_NO_SMARTCARD;
		}
	}

	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (pioSendPci->dwProtocol != SCARD_PROTOCOL_ANY_OLD)
		{
			if (pioSendPci->dwProtocol != rContext->readerState->cardProtocol)
			{
				return SCARD_E_PROTO_MISMATCH;
			}
		}
	}

	if (cbSendLength > MAX_BUFFER_SIZE)
	{
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	/*
	 * Removed - a user may allocate a larger buffer if ( dwRxLength >
	 * MAX_BUFFER_SIZE ) { return SCARD_E_INSUFFICIENT_BUFFER; }
	 */

	/*
	 * Quick fix: PC/SC starts at 1 for bit masking but the IFD_Handler
	 * just wants 0 or 1
	 */

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_T0)
	{
		sSendPci.Protocol = 0;
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_T1)
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

		for (i = 0 ; prot != 1 ; i++)
			prot >>= 1;

		sSendPci.Protocol = i;
	}

	sSendPci.Length = pioSendPci->cbPciLength;

	/* the protocol number is decoded a few lines above */
	DebugLogB("Send Protocol: T=%d", sSendPci.Protocol);

	tempRxLength = dwRxLength;

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
	{
		rv = IFDControl_v2(rContext, (PUCHAR) pbSendBuffer, cbSendLength,
			pbRecvBuffer, &dwRxLength);
	} else
	{
		rv = IFDTransmit(rContext, sSendPci, (PUCHAR) pbSendBuffer,
			cbSendLength, pbRecvBuffer, &dwRxLength, &sRecvPci);
	}

	if (pioRecvPci)
	{
		pioRecvPci->dwProtocol = sRecvPci.Protocol;
		pioRecvPci->cbPciLength = sRecvPci.Length;
	}

	/*
	 * Check for any errors that might have occurred
	 */

	if (rv != SCARD_S_SUCCESS)
	{
		*pcbRecvLength = 0;
		DebugLogB("Card not transacted: %ld", rv);
		return SCARD_E_NOT_TRANSACTED;
	}

	/*
	 * Available is less than received
	 */
	if (tempRxLength < dwRxLength)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	if (dwRxLength > MAX_BUFFER_SIZE)
	{
		*pcbRecvLength = 0;
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	/*
	 * Successful return
	 */
	*pcbRecvLength = dwRxLength;
	return SCARD_S_SUCCESS;
}

LONG SCardListReaders(SCARDCONTEXT hContext, LPCTSTR mszGroups,
	LPTSTR mszReaders, LPDWORD pcchReaders)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

LONG SCardCancel(SCARDCONTEXT hContext)
{
	/*
	 * Client side function
	 */
	return SCARD_S_SUCCESS;
}

