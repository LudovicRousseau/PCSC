/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : winscard.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles smartcard reader communications. 
	             This is the heart of the M$ smartcard API.
	            
********************************************************************/

#include <stdlib.h>
#include <sys/time.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "winscard.h"
#include "readerfactory.h"
#include "prothandler.h"
#include "ifdhandler.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "debuglog.h"
#include "configfile.h"
#include "sys_generic.h"

/*
 * Some defines for context stack 
 */
#define SCARD_LAST_CONTEXT       1
#define SCARD_NO_CONTEXT         0
#define SCARD_EXCLUSIVE_CONTEXT -1
#define SCARD_NO_LOCK            0

SCARD_IO_REQUEST g_rgSCardT0Pci, g_rgSCardT1Pci, g_rgSCardRawPci;

LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{

	LONG rv;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	/*
	 * Check for NULL pointer 
	 */
	if (phContext == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

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

	DebugLogB("SCardEstablishContext: Establishing Context: %d",
		*phContext);

	return SCARD_S_SUCCESS;
}

LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	/*
	 * Nothing to do here RPC layer will handle this 
	 */

	DebugLogB("SCardReleaseContext: Releasing Context: %d", hContext);

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

LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	LONG rv;
	PREADER_CONTEXT rContext;
	UCHAR pucAtr[MAX_ATR_SIZE], ucAvailable;
	DWORD dwAtrLength, dwState, dwStatus;
	DWORD dwReaderLen, dwProtocol;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rContext = 0;
	ucAvailable = 0;
	dwAtrLength = 0;
	dwState = 0;
	dwStatus = 0;
	dwReaderLen = 0;
	dwProtocol = 0;
	memset(pucAtr, 0x00, MAX_ATR_SIZE);

	/*
	 * Check for NULL parameters 
	 */
	if (szReader == 0 || phCard == 0 || pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	} else
	{
		*phCard = 0;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_PROTO_MISMATCH;
	}

	if (dwShareMode != SCARD_SHARE_EXCLUSIVE &&
		dwShareMode != SCARD_SHARE_SHARED &&
		dwShareMode != SCARD_SHARE_DIRECT)
	{

		return SCARD_E_INVALID_VALUE;
	}

	DebugLogB("SCardConnect: Attempting Connect to %s", szReader);

	rv = RFReaderInfo((LPSTR) szReader, &rContext);

	if (rv != SCARD_S_SUCCESS)
	{
		DebugLogB("SCardConnect: Reader %s Not Found", szReader);
		return rv;
	}

	/*
	 * Make sure the reader is working properly 
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

  /*******************************************/
	/*
	 * This section checks for simple errors 
	 */
  /*******************************************/

	/*
	 * Connect if not exclusive mode 
	 */
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
	{
		DebugLogA("SCardConnect: Error Reader Exclusive");
		return SCARD_E_SHARING_VIOLATION;
	}

  /*******************************************/
	/*
	 * This section tries to determine the 
	 */
	/*
	 * presence of a card or not 
	 */
  /*******************************************/
	dwStatus = rContext->dwStatus;

	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		if (!(dwStatus & SCARD_PRESENT))
		{
			DebugLogA("SCardConnect: Card Not Inserted");
			return SCARD_E_NO_SMARTCARD;
		}
	}

  /*******************************************/
	/*
	 * This section tries to decode the ATR 
	 */
	/*
	 * and set up which protocol to use 
	 */
  /*******************************************/

	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
	{
		rContext->dwProtocol = -1;
	} else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			memcpy(pucAtr, rContext->ucAtr, rContext->dwAtrLen);
			dwAtrLength = rContext->dwAtrLen;

			rContext->dwProtocol =
				PHGetDefaultProtocol(pucAtr, dwAtrLength);
			ucAvailable = PHGetAvailableProtocols(pucAtr, dwAtrLength);

			/*
			 * If it is set to any let it do any of the protocols 
			 */
			if (dwPreferredProtocols & SCARD_PROTOCOL_ANY)
			{
				rContext->dwProtocol = PHSetProtocol(rContext, ucAvailable,
					ucAvailable);
			} else
			{
				rContext->dwProtocol =
					PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable);
				if (rContext->dwProtocol == -1)
				{
					return SCARD_E_PROTO_MISMATCH;
				}
			}
		}
	}

	*pdwActiveProtocol = rContext->dwProtocol;

	DebugLogB("SCardConnect: Active Protocol: %d", *pdwActiveProtocol);

	/*
	 * Prepare the SCARDHANDLE identity 
	 */
	*phCard = RFCreateReaderHandle(rContext);

	DebugLogB("SCardConnect: hCard Identity: %x", *phCard);

  /*******************************************/
	/*
	 * This section tries to set up the 
	 */
	/*
	 * exclusivity modes. -1 is exclusive 
	 */
  /*******************************************/

	if (dwShareMode == SCARD_SHARE_EXCLUSIVE)
	{
		if (rContext->dwContexts == SCARD_NO_CONTEXT)
		{
			rContext->dwContexts = SCARD_EXCLUSIVE_CONTEXT;
			RFLockSharing(*phCard);
		} else
		{
			RFDestroyReaderHandle(*phCard);
			*phCard = 0;
			return SCARD_E_SHARING_VIOLATION;
		}
	} else
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
		{
			rContext->dwContexts = SCARD_NO_CONTEXT;
		} else if (rContext->dwContexts > SCARD_NO_CONTEXT)
		{
			rContext->dwContexts -= 1;
		}
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
	PREADER_CONTEXT rContext;
	UCHAR pucAtr[MAX_ATR_SIZE], ucAvailable;
	DWORD dwAtrLength, dwAction = 0;

        DebugLogA("SCardReconnect: Attempting reconnect to token.");

	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (dwInitialization != SCARD_LEAVE_CARD &&
		dwInitialization != SCARD_RESET_CARD &&
		dwInitialization != SCARD_UNPOWER_CARD)
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (dwShareMode != SCARD_SHARE_SHARED &&
		dwShareMode != SCARD_SHARE_EXCLUSIVE &&
		dwShareMode != SCARD_SHARE_DIRECT)
	{

		return SCARD_E_INVALID_VALUE;
	}

	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
		!(dwPreferredProtocols & SCARD_PROTOCOL_ANY))
	{

		return SCARD_E_PROTO_MISMATCH;
	}

	if (pdwActiveProtocol == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure the reader is working properly 
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure no one has a lock on this reader 
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Handle the dwInitialization 
	 */

	switch (dwInitialization)
	{
	case SCARD_LEAVE_CARD:	/* Do nothing here */
		break;
	case SCARD_UNPOWER_CARD:
		dwAction = IFD_POWER_DOWN;
		break;
	case SCARD_RESET_CARD:
		dwAction = IFD_RESET;
		break;
	default:
		return SCARD_E_INVALID_VALUE;
	};

	/*
	 * Thread handles powering so just reset instead 
	 */
	if (dwInitialization == SCARD_UNPOWER_CARD)
	{
		dwAction = IFD_RESET;
	}

	/*
	 * RFUnblockReader( rContext ); FIX - this doesn't work 
	 */

	if (dwInitialization == SCARD_RESET_CARD ||
		dwInitialization == SCARD_UNPOWER_CARD)
	{
		/*
		 * Not doing this could result in deadlock 
		 */
		if (RFCheckReaderEventState(rContext, hCard) != SCARD_W_RESET_CARD)
		{
			rv = IFDPowerICC(rContext, dwAction, rContext->ucAtr,
				&rContext->dwAtrLen);

			/*
			 * Notify the card has been reset 
			 */
			RFSetReaderEventState(rContext, SCARD_RESET);

			/*
			 * Set up the status bit masks on dwStatus 
			 */
			if (rv == SCARD_S_SUCCESS)
			{
				rContext->dwStatus |= SCARD_PRESENT;
				rContext->dwStatus &= ~SCARD_ABSENT;
				rContext->dwStatus |= SCARD_POWERED;
				rContext->dwStatus |= SCARD_NEGOTIABLE;
				rContext->dwStatus &= ~SCARD_SPECIFIC;
				rContext->dwStatus &= ~SCARD_SWALLOWED;
				rContext->dwStatus &= ~SCARD_UNKNOWN;
			} else
			{
				rContext->dwStatus |= SCARD_PRESENT;
				rContext->dwStatus &= ~SCARD_ABSENT;
				rContext->dwStatus |= SCARD_SWALLOWED;
				rContext->dwStatus &= ~SCARD_POWERED;
				rContext->dwStatus &= ~SCARD_NEGOTIABLE;
				rContext->dwStatus &= ~SCARD_SPECIFIC;
				rContext->dwStatus &= ~SCARD_UNKNOWN;
				rContext->dwProtocol = 0;
				rContext->dwAtrLen = 0;
			}

			if (rContext->dwAtrLen > 0)
				DebugLogA("SCardReconnect: Reset complete.");
			else
				DebugLogA("SCardReconnect: Error resetting card.");
		}

	} else if (dwInitialization == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing 
		 */
	}

	/*
	 * Handle the dwActive/Preferred Protocols 
	 */
	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
	{
		rContext->dwProtocol = -1;
	} else
	{
		if (dwShareMode != SCARD_SHARE_DIRECT)
		{
			memcpy(pucAtr, rContext->ucAtr, rContext->dwAtrLen);
			dwAtrLength = rContext->dwAtrLen;

			rContext->dwProtocol =
				PHGetDefaultProtocol(pucAtr, dwAtrLength);
			ucAvailable = PHGetAvailableProtocols(pucAtr, dwAtrLength);

			/*
			 * If it is set to any let it do any of the protocols 
			 */
			if (dwPreferredProtocols & SCARD_PROTOCOL_ANY)
			{
				rContext->dwProtocol =
					PHSetProtocol(rContext,
					SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, ucAvailable);
			} else
			{
				rContext->dwProtocol =
					PHSetProtocol(rContext, dwPreferredProtocols,
					ucAvailable);
				if (rContext->dwProtocol == -1)
				{
					return SCARD_E_PROTO_MISMATCH;
				}
			}
		}
	}

	*pdwActiveProtocol = rContext->dwProtocol;

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
	{
		return SCARD_E_INVALID_VALUE;
	}

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
	UCHAR controlBuffer[5];
	UCHAR receiveBuffer[MAX_BUFFER_SIZE];
	PREADER_CONTEXT rContext;
	DWORD dwAction, dwAtrLen, receiveLength;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rContext = 0;
	dwAction = 0;
	dwAtrLen = 0;
	receiveLength = 0;

	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	switch (dwDisposition)
	{
	case SCARD_LEAVE_CARD:	/* Do nothing here */
		break;
	case SCARD_UNPOWER_CARD:
		dwAction = IFD_POWER_DOWN;
		break;
	case SCARD_RESET_CARD:
		dwAction = IFD_RESET;
		break;
	case SCARD_EJECT_CARD:
		break;
	default:
		return SCARD_E_INVALID_VALUE;
	};

	/*
	 * Thread handles powering so just reset instead 
	 */
	if (dwDisposition == SCARD_UNPOWER_CARD)
	{
		dwAction = IFD_RESET;
	}

	/*
	 * Unlock any blocks on this context 
	 */
	RFUnlockSharing(hCard);

	DebugLogB("SCardDisconnect: Active Contexts: %d",
		rContext->dwContexts);

	/*
	 * RFUnblockReader( rContext ); FIX - this doesn't work 
	 */

	/*
	 * Allow RESET only if no other application holds a lock 
	 */

	/*
	 * Deprecated - any app can reset according to M$ 
	 */
	/*
	 * if ( RFCheckSharing( hCard ) == SCARD_S_SUCCESS ) { 
	 */
	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{

		/*
		 * Currently pcsc-lite keeps the card powered constantly 
		 */
		rv = IFDPowerICC(rContext, dwAction, rContext->ucAtr,
			&rContext->dwAtrLen);

		/*
		 * Notify the card has been reset 
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on dwStatus 
		 */
		if (rv == SCARD_S_SUCCESS)
		{
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_POWERED;
			rContext->dwStatus |= SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
		} else
		{
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_POWERED;
			rContext->dwStatus &= ~SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
			rContext->dwProtocol = 0;
			rContext->dwAtrLen = 0;
		}

		if (rContext->dwAtrLen > 0)
			DebugLogA("SCardDisconnect: Reset complete.");
		else
			DebugLogA("SCardDisconnect: Error resetting card.");

	} else if (dwDisposition == SCARD_EJECT_CARD)
	{
		/*
		 * Set up the CTBCS command for Eject ICC 
		 */
		controlBuffer[0] = 0x20;
		controlBuffer[1] = 0x15;
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				DebugLogA("SCardDisconnect: Card ejected successfully.");
				/*
				 * Successful 
				 */
			} else
			{
				DebugLogA("SCardDisconnect: Error ejecting card.");
			}
		} else
		{
			DebugLogA("SCardDisconnect: Error ejecting card.");
		}

	} else if (dwDisposition == SCARD_LEAVE_CARD)
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
	{
		rContext->dwContexts = 0;
	}

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

	/*
	 * Zero out everything 
	 */
	rv = 0;

	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context 
	 */
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure the reader is working properly 
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure some event has not occurred 
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFLockSharing(hCard);

	DebugLogB("SCardBeginTransaction: Status: %d.", rv);

	return rv;
}

LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{

	LONG rv;
	PREADER_CONTEXT rContext;
	UCHAR controlBuffer[5];
	UCHAR receiveBuffer[MAX_BUFFER_SIZE];
	DWORD dwAction, receiveLength;

	/*
	 * Zero out everything 
	 */
	rContext = 0;
	rv = 0;
	dwAction = 0;
	receiveLength = 0;

	/*
	 * Ignoring dwDisposition for now 
	 */
	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	switch (dwDisposition)
	{
	case SCARD_LEAVE_CARD:	/* Do nothing here */
		break;
	case SCARD_UNPOWER_CARD:
		dwAction = IFD_POWER_DOWN;
		break;
	case SCARD_RESET_CARD:
		dwAction = IFD_RESET;
		break;
	case SCARD_EJECT_CARD:
		break;
	default:
		return SCARD_E_INVALID_VALUE;
	};

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context 
	 */
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure some event has not occurred 
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Thread handles powering so just reset instead 
	 */
	if (dwDisposition == SCARD_UNPOWER_CARD)
	{
		dwAction = IFD_RESET;
	}

	if (dwDisposition == SCARD_RESET_CARD ||
		dwDisposition == SCARD_UNPOWER_CARD)
	{

		/*
		 * Currently pcsc-lite keeps the card always powered 
		 */
		rv = IFDPowerICC(rContext, dwAction, rContext->ucAtr,
			&rContext->dwAtrLen);

		/*
		 * Notify the card has been reset 
		 */
		RFSetReaderEventState(rContext, SCARD_RESET);

		/*
		 * Set up the status bit masks on dwStatus 
		 */
		if (rv == SCARD_S_SUCCESS)
		{
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_POWERED;
			rContext->dwStatus |= SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
		} else
		{
			rContext->dwStatus |= SCARD_PRESENT;
			rContext->dwStatus &= ~SCARD_ABSENT;
			rContext->dwStatus |= SCARD_SWALLOWED;
			rContext->dwStatus &= ~SCARD_POWERED;
			rContext->dwStatus &= ~SCARD_NEGOTIABLE;
			rContext->dwStatus &= ~SCARD_SPECIFIC;
			rContext->dwStatus &= ~SCARD_UNKNOWN;
			rContext->dwProtocol = 0;
			rContext->dwAtrLen = 0;
		}

		if (rContext->dwAtrLen > 0)
			DebugLogA("SCardEndTransaction: Reset complete.");
		else
			DebugLogA("SCardEndTransaction: Error resetting card.");

	} else if (dwDisposition == SCARD_EJECT_CARD)
	{
		/*
		 * Set up the CTBCS command for Eject ICC 
		 */
		controlBuffer[0] = 0x20;
		controlBuffer[1] = 0x15;
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;
		receiveLength = 2;
		rv = IFDControl(rContext, controlBuffer, 5, receiveBuffer,
			&receiveLength);

		if (rv == SCARD_S_SUCCESS)
		{
			if (receiveLength == 2 && receiveBuffer[0] == 0x90)
			{
				DebugLogA
					("SCardEndTransaction: Card ejected successfully.");
				/*
				 * Successful 
				 */
			} else
			{
				DebugLogA("SCardEndTransaction: Error ejecting card.");
			}
		} else
		{
			DebugLogA("SCardEndTransaction: Error ejecting card.");
		}

	} else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing 
		 */
	}

	/*
	 * Unlock any blocks on this context 
	 */
	RFUnlockSharing(hCard);

	DebugLogB("SCardEndTransaction: Status: %d.", rv);

	return rv;
}

LONG SCardCancelTransaction(SCARDHANDLE hCard)
{

	LONG rv;
	PREADER_CONTEXT rContext;

	/*
	 * Zero out everything 
	 */
	rContext = 0;
	rv = 0;

	/*
	 * Ignoring dwDisposition for now 
	 */
	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context 
	 */
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure some event has not occurred 
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFUnlockSharing(hCard);

	DebugLogB("SCardCancelTransaction: Status: %d.", rv);

	return rv;
}

LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{

	LONG rv;
	PREADER_CONTEXT rContext;

	/*
	 * Zero out everything 
	 */
	rContext = 0;
	rv = 0;

	rv = RFReaderInfoById(hCard, &rContext);

	/*
	 * Cannot find the hCard in this context 
	 */
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * This is a client side function however the server maintains the
	 * list of events between applications so it must be passed through to 
	 * obtain this event if it has occurred 
	 */

	/*
	 * Make sure some event has not occurred 
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure the reader is working properly 
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	return SCARD_S_SUCCESS;
}

LONG SCardGetStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout,
	LPSCARD_READERSTATE_A rgReaderStates, DWORD cReaders)
{

	/*
	 * Client side function 
	 */
	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE hCard, LPCBYTE pbSendBuffer,
	DWORD cbSendLength, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{

	/*
	 * This is not used.  SCardControl is passed through SCardTransmit.
	 * This is here to make the compiler happy. 
	 */

	return SCARD_S_SUCCESS;
}

LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{

	LONG rv;
	PREADER_CONTEXT rContext;
	SCARD_IO_HEADER sSendPci, sRecvPci;
	DWORD dwRxLength, tempRxLength;

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rContext = 0;
	dwRxLength = 0;
	tempRxLength = 0;

	if (pcbRecvLength == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	dwRxLength = *pcbRecvLength;
	*pcbRecvLength = 0;

	if (hCard == 0)
	{
		return SCARD_E_INVALID_HANDLE;
	}

	if (pbSendBuffer == 0 || pbRecvBuffer == 0 || pioRecvPci == 0 ||
		pioSendPci == 0)
	{
		return SCARD_E_INVALID_PARAMETER;
	}

	/*
	 * Must at least have 2 status words even for SCardControl 
	 */
	if (dwRxLength < 2)
	{
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	/*
	 * Make sure no one has a lock on this reader 
	 */
	if ((rv = RFCheckSharing(hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFReaderInfoById(hCard, &rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure the reader is working properly 
	 */
	rv = RFCheckReaderStatus(rContext);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	rv = RFFindReaderHandle(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Make sure some event has not occurred 
	 */
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
	{
		return rv;
	}

	/*
	 * Check for some common errors 
	 */
	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (rContext->dwStatus & SCARD_ABSENT)
		{
			return SCARD_E_NO_SMARTCARD;
		}
	}

	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_RAW)
	{
		if (pioSendPci->dwProtocol != SCARD_PROTOCOL_ANY)
		{
			if (pioSendPci->dwProtocol != rContext->dwProtocol)
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
	} else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_ANY)
	{
		sSendPci.Protocol = rContext->dwProtocol;
	}

	sSendPci.Length = pioSendPci->cbPciLength;

	DebugLogB("SCardTransmit: Send Protocol: %d", sSendPci.Protocol);

	tempRxLength = dwRxLength;

	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW)
	{
		rv = IFDControl(rContext, (PUCHAR) pbSendBuffer, cbSendLength,
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

LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
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
