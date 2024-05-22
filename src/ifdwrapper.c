/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
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
 * @brief This wraps the dynamic ifdhandler functions.
 */

#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "misc.h"
#include "pcscd.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "utils.h"

#ifdef PCSCLITE_STATIC_DRIVER
/* check that either IFDHANDLERv2 or IFDHANDLERv3 is
 * defined */
  #if ! (defined(IFDHANDLERv2) || defined(IFDHANDLERv3))
  #error IFDHANDLER version not defined
  #endif
#endif

/**
 * Set the protocol type selection (PTS).
 * This function sets the appropriate protocol to be used on the card.
 */
RESPONSECODE IFDSetPTS(READER_CONTEXT * rContext, DWORD dwProtocol,
	UCHAR ucFlags, UCHAR ucPTS1, UCHAR ucPTS2, UCHAR ucPTS3)
{
	RESPONSECODE rv;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_set_protocol_parameters) (DWORD, DWORD, UCHAR,
		UCHAR, UCHAR, UCHAR) = NULL;

	IFDH_set_protocol_parameters = (RESPONSECODE(*)(DWORD, DWORD, UCHAR,
		UCHAR, UCHAR, UCHAR))
		rContext->psFunctions.psFunctions_v2.pvfSetProtocolParameters;

	if (NULL == IFDH_set_protocol_parameters)
		return SCARD_E_UNSUPPORTED_FEATURE;
#endif

	/*
	 * Locking is done in winscard.c SCardConnect() and SCardReconnect()
	 *
	 * This avoids to renegotiate the protocol and confuse the card
	 * Error returned by CCID driver is: CCID_Receive Procedure byte conflict
	 */

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_set_protocol_parameters) (rContext->slot,
		dwProtocol, ucFlags, ucPTS1, ucPTS2, ucPTS3);
#else
	rv = IFDHSetProtocolParameters(rContext->slot, dwProtocol, ucFlags,
		ucPTS1, ucPTS2, ucPTS3);
#endif

	return rv;
}

/**
 * Open a communication channel to the IFD.
 */
RESPONSECODE IFDOpenIFD(READER_CONTEXT * rContext)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_create_channel) (DWORD, DWORD) = NULL;
	RESPONSECODE(*IFDH_create_channel_by_name) (DWORD, LPSTR) = NULL;

	if (rContext->version == IFD_HVERSION_2_0)
		IFDH_create_channel =
			rContext->psFunctions.psFunctions_v2.pvfCreateChannel;
	else
	{
		IFDH_create_channel =
			rContext->psFunctions.psFunctions_v3.pvfCreateChannel;
		IFDH_create_channel_by_name =
			rContext->psFunctions.psFunctions_v3.pvfCreateChannelByName;
	}
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->version == IFD_HVERSION_2_0)
	{
		rv = (*IFDH_create_channel) (rContext->slot, rContext->port);
	} else
	{
		/* use device name only if defined */
		if (rContext->device[0] != '\0')
			rv = (*IFDH_create_channel_by_name) (rContext->slot, rContext->device);
		else
			rv = (*IFDH_create_channel) (rContext->slot, rContext->port);
	}
#else
#if defined(IFDHANDLERv2)
	rv = IFDHCreateChannel(rContext->slot, rContext->port);
#else
	{
		/* Use device name only if defined */
		if (rContext->device[0] != '\0')
			rv = IFDHCreateChannelByName(rContext->slot, rContext->device);
		else
			rv = IFDHCreateChannel(rContext->slot, rContext->port);
	}
#endif
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	return rv;
}

/**
 * Close a communication channel to the IFD.
 */
RESPONSECODE IFDCloseIFD(READER_CONTEXT * rContext)
{
	RESPONSECODE rv;
	int repeat;
	bool do_unlock = true;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_close_channel) (DWORD) = NULL;

	IFDH_close_channel = rContext->psFunctions.psFunctions_v2.pvfCloseChannel;
#endif

	/* TRY TO LOCK THIS CODE REGION */
	repeat = 5;
again:
	rv = pthread_mutex_trylock(rContext->mMutex);
	if (EBUSY == rv)
	{
		Log1(PCSC_LOG_ERROR, "Locking failed");
		repeat--;
		if (repeat)
		{
			(void)SYS_USleep(100*1000);	/* 100 ms */
			goto again;
		}
		else
			/* locking failed but we need to close the IFD */
			do_unlock = false;
	}

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_close_channel) (rContext->slot);
#else
	rv = IFDHCloseChannel(rContext->slot);
#endif

	/* END OF LOCKED REGION */
	if (do_unlock)
		(void)pthread_mutex_unlock(rContext->mMutex);

	return rv;
}

/**
 * Set capabilities in the reader.
 */
RESPONSECODE IFDSetCapabilities(READER_CONTEXT * rContext, DWORD dwTag,
			DWORD dwLength, PUCHAR pucValue)
{
	RESPONSECODE rv;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_set_capabilities) (DWORD, DWORD, DWORD, PUCHAR) = NULL;

	IFDH_set_capabilities = rContext->psFunctions.psFunctions_v2.pvfSetCapabilities;
#endif

	/*
	 * Let the calling function lock this otherwise a deadlock will
	 * result
	 */

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_set_capabilities) (rContext->slot, dwTag,
			dwLength, pucValue);
#else
	rv = IFDHSetCapabilities(rContext->slot, dwTag, dwLength, pucValue);
#endif

	return rv;
}

/**
 * Gets capabilities in the reader.
 * Other functions int this file will call
 * the driver directly to not cause a deadlock.
 */
RESPONSECODE IFDGetCapabilities(READER_CONTEXT * rContext, DWORD dwTag,
	PDWORD pdwLength, PUCHAR pucValue)
{
	RESPONSECODE rv;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_get_capabilities) (DWORD, DWORD, PDWORD, /*@out@*/ PUCHAR) = NULL;

	IFDH_get_capabilities =
		rContext->psFunctions.psFunctions_v2.pvfGetCapabilities;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_get_capabilities) (rContext->slot, dwTag, pdwLength, pucValue);
#else
	rv = IFDHGetCapabilities(rContext->slot, dwTag, pdwLength, pucValue);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	return rv;
}

/**
 * Power up/down or reset's an ICC located in the IFD.
 */
RESPONSECODE IFDPowerICC(READER_CONTEXT * rContext, DWORD dwAction,
	PUCHAR pucAtr, PDWORD pdwAtrLen)
{
	RESPONSECODE rv;
	DWORD dwStatus;
	UCHAR dummyAtr[MAX_ATR_SIZE];
	DWORD dummyAtrLen = sizeof(dummyAtr);

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_power_icc) (DWORD, DWORD, PUCHAR, PDWORD) = NULL;
#endif

	/*
	 * Zero out everything
	 */
	dwStatus = 0;

	if (NULL == pucAtr)
		pucAtr = dummyAtr;
	if (NULL == pdwAtrLen)
		pdwAtrLen = &dummyAtrLen;

	/*
	 * Check that the card is inserted first
	 */
	rv = IFDStatusICC(rContext, &dwStatus);
	if (rv != SCARD_S_SUCCESS)
		return rv;

	if (dwStatus & SCARD_ABSENT)
		return SCARD_W_REMOVED_CARD;
#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_power_icc = rContext->psFunctions.psFunctions_v2.pvfPowerICC;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_power_icc) (rContext->slot, dwAction, pucAtr, pdwAtrLen);
#else
	rv = IFDHPowerICC(rContext->slot, dwAction, pucAtr, pdwAtrLen);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	/* use clean values in case of error */
	if (rv != IFD_SUCCESS)
	{
		*pdwAtrLen = 0;
		pucAtr[0] = '\0';

		if (rv == IFD_NO_SUCH_DEVICE)
		{
			(void)SendHotplugSignal();
			return SCARD_E_READER_UNAVAILABLE;
		}

		return SCARD_E_NOT_TRANSACTED;
	}

	return rv;
}

/**
 * Provide statistical information about the IFD and ICC including insertions,
 * atr, powering status/etc.
 */
LONG IFDStatusICC(READER_CONTEXT * rContext, PDWORD pdwStatus)
{
	RESPONSECODE rv;
	DWORD dwCardStatus = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_icc_presence) (DWORD) = NULL;

	IFDH_icc_presence = rContext->psFunctions.psFunctions_v2.pvfICCPresence;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_icc_presence) (rContext->slot);
#else
	rv = IFDHICCPresence(rContext->slot);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	if (rv == IFD_SUCCESS || rv == IFD_ICC_PRESENT)
		dwCardStatus |= SCARD_PRESENT;
	else
		if (rv == IFD_ICC_NOT_PRESENT)
			dwCardStatus |= SCARD_ABSENT;
		else
		{
			Log2(PCSC_LOG_ERROR, "Card not transacted: %ld", rv);
			*pdwStatus = SCARD_UNKNOWN;

			if (rv == IFD_NO_SUCH_DEVICE)
			{
				(void)SendHotplugSignal();
				return SCARD_E_READER_UNAVAILABLE;
			}

			return SCARD_E_NOT_TRANSACTED;
		}

	*pdwStatus = dwCardStatus;

	return SCARD_S_SUCCESS;
}

/*
 * Function: IFDControl Purpose : This function provides a means for
 * toggling a specific action on the reader such as swallow, eject,
 * biometric.
 */

/*
 * Valid only for IFDHandler version 2.0
 */

LONG IFDControl_v2(READER_CONTEXT * rContext, PUCHAR TxBuffer,
	DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_control_v2) (DWORD, PUCHAR, DWORD, /*@out@*/ PUCHAR,
		PDWORD);
#endif

	if (rContext->version != IFD_HVERSION_2_0)
		return SCARD_E_UNSUPPORTED_FEATURE;

#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_control_v2 = rContext->psFunctions.psFunctions_v2.pvfControl;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_control_v2) (rContext->slot, TxBuffer, TxLength,
		RxBuffer, RxLength);
#elif defined(IFDHANDLERv2)
	rv = IFDHControl(rContext->slot, TxBuffer, TxLength,
		RxBuffer, RxLength);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
	{
		Log2(PCSC_LOG_ERROR, "Card not transacted: %ld", rv);
		LogXxd(PCSC_LOG_DEBUG, "TxBuffer ", TxBuffer, TxLength);
		LogXxd(PCSC_LOG_DEBUG, "RxBuffer ", RxBuffer, *RxLength);
		return SCARD_E_NOT_TRANSACTED;
	}
}

/**
 * Provide a means for toggling a specific action on the reader such as
 * swallow, eject, biometric.
 */

/*
 * Valid only for IFDHandler version 3.0 and up
 */

LONG IFDControl(READER_CONTEXT * rContext, DWORD ControlCode,
	LPCVOID TxBuffer, DWORD TxLength, LPVOID RxBuffer, DWORD RxLength,
	LPDWORD BytesReturned)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_control) (DWORD, DWORD, LPCVOID, DWORD, LPVOID, DWORD, LPDWORD);
#endif

	if (rContext->version < IFD_HVERSION_3_0)
		return SCARD_E_UNSUPPORTED_FEATURE;

#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_control = rContext->psFunctions.psFunctions_v3.pvfControl;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_control) (rContext->slot, ControlCode, TxBuffer,
		TxLength, RxBuffer, RxLength, BytesReturned);
#elif defined(IFDHANDLERv3)
	rv = IFDHControl(rContext->slot, ControlCode, TxBuffer,
		TxLength, RxBuffer, RxLength, BytesReturned);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
	{
		Log2(PCSC_LOG_ERROR, "Card not transacted: %ld", rv);
		Log3(PCSC_LOG_DEBUG, "ControlCode: 0x%.8lX BytesReturned: %ld",
			ControlCode, *BytesReturned);
		LogXxd(PCSC_LOG_DEBUG, "TxBuffer ", TxBuffer, TxLength);
		LogXxd(PCSC_LOG_DEBUG, "RxBuffer ", RxBuffer, *BytesReturned);

		if (rv == IFD_NO_SUCH_DEVICE)
		{
			(void)SendHotplugSignal();
			return SCARD_E_READER_UNAVAILABLE;
		}

		if ((IFD_ERROR_NOT_SUPPORTED == rv) || (IFD_NOT_SUPPORTED == rv))
			return SCARD_E_UNSUPPORTED_FEATURE;

        if (IFD_ERROR_INSUFFICIENT_BUFFER ==rv)
            return SCARD_E_INSUFFICIENT_BUFFER;

		return SCARD_E_NOT_TRANSACTED;
	}
}

/**
 * Transmit an APDU to the ICC.
 */
LONG IFDTransmit(READER_CONTEXT * rContext, SCARD_IO_HEADER pioTxPci,
	PUCHAR pucTxBuffer, DWORD dwTxLength, PUCHAR pucRxBuffer,
	PDWORD pdwRxLength, PSCARD_IO_HEADER pioRxPci)
{
	RESPONSECODE rv;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_transmit_to_icc) (DWORD, SCARD_IO_HEADER, PUCHAR,
		DWORD, /*@out@*/ PUCHAR, PDWORD, PSCARD_IO_HEADER) = NULL;
#endif

	/* log the APDU */
	DebugLogCategory(DEBUG_CATEGORY_APDU, pucTxBuffer, dwTxLength);

#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_transmit_to_icc =
		rContext->psFunctions.psFunctions_v2.pvfTransmitToICC;
#endif

	/* LOCK THIS CODE REGION */
	(void)pthread_mutex_lock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_transmit_to_icc) (rContext->slot, pioTxPci, (LPBYTE)
		pucTxBuffer, dwTxLength, pucRxBuffer, pdwRxLength, pioRxPci);
#else
	rv = IFDHTransmitToICC(rContext->slot, pioTxPci,
		(LPBYTE) pucTxBuffer, dwTxLength,
		pucRxBuffer, pdwRxLength, pioRxPci);
#endif

	/* END OF LOCKED REGION */
	(void)pthread_mutex_unlock(rContext->mMutex);

	/* log the returned status word */
	DebugLogCategory(DEBUG_CATEGORY_SW, pucRxBuffer, *pdwRxLength);

	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
	{
		Log2(PCSC_LOG_ERROR, "Card not transacted: %ld", rv);

		if (rv == IFD_NO_SUCH_DEVICE)
		{
			(void)SendHotplugSignal();
			return SCARD_E_READER_UNAVAILABLE;
		}

		if (rv == IFD_ICC_NOT_PRESENT)
			return SCARD_E_NO_SMARTCARD;

		return SCARD_E_NOT_TRANSACTED;
	}
}

