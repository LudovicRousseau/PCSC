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
 * @brief This wraps the dynamic ifdhandler functions.
 */

#include <errno.h>
#include "config.h"
#include "misc.h"
#include "pcscd.h"
#include "ifdhandler.h"
#include "debuglog.h"
#include "readerfactory.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"
#include "utils.h"

#undef PCSCLITE_STATIC_DRIVER

/**
 * Set the protocol type selection (PTS).
 * This function sets the appropriate protocol to be used on the card.
 */
LONG IFDSetPTS(PREADER_CONTEXT rContext, DWORD dwProtocol, UCHAR ucFlags,
	UCHAR ucPTS1, UCHAR ucPTS2, UCHAR ucPTS3)
{
	RESPONSECODE rv = IFD_SUCCESS;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_set_protocol_parameters) (DWORD, UCHAR, UCHAR,
		UCHAR, UCHAR) = NULL;
	RESPONSECODE(*IFDH_set_protocol_parameters) (DWORD, DWORD, UCHAR,
		UCHAR, UCHAR, UCHAR) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		IFD_set_protocol_parameters = (RESPONSECODE(*)(DWORD, UCHAR, UCHAR,
			UCHAR, UCHAR)) rContext->psFunctions.psFunctions_v1.pvfSetProtocolParameters;

		if (NULL == IFD_set_protocol_parameters)
			return SCARD_E_UNSUPPORTED_FEATURE;
	}
	else
	{
		IFDH_set_protocol_parameters = (RESPONSECODE(*)(DWORD, DWORD, UCHAR,
			UCHAR, UCHAR, UCHAR))
			rContext->psFunctions.psFunctions_v2.pvfSetProtocolParameters;

		if (NULL == IFDH_set_protocol_parameters)
			return SCARD_E_UNSUPPORTED_FEATURE;
	}
#endif

	/*
	 * Locking is done in winscard.c SCardConnect() and SCardReconnect()
	 *
	 * This avoids to renegociate the protocol and confuse the card
	 * Error returned by CCID driver is: CCID_Receive Procedure byte conflict
	 */

	ucValue[0] = rContext->dwSlot;

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        (void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
	        rv = (*IFD_set_protocol_parameters) (dwProtocol,
			ucFlags, ucPTS1, ucPTS2, ucPTS3);
	}
	else
	{
		rv = (*IFDH_set_protocol_parameters) (rContext->dwSlot,
						      dwProtocol,
						      ucFlags, ucPTS1,
						      ucPTS2, ucPTS3);
	}
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        (void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Set_Protocol_Parameters(dwProtocol, ucFlags, ucPTS1,
			ucPTS2, ucPTS3);
	}
	else
	{
		rv = IFDHSetProtocolParameters(rContext->dwSlot, dwProtocol,
			ucFlags, ucPTS1, ucPTS2, ucPTS3);
	}
#endif

	return rv;
}

/**
 * Open a communication channel to the IFD.
 */
LONG IFDOpenIFD(PREADER_CONTEXT rContext)
{
	RESPONSECODE rv = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IO_create_channel) (DWORD) = NULL;
	RESPONSECODE(*IFDH_create_channel) (DWORD, DWORD) = NULL;
	RESPONSECODE(*IFDH_create_channel_by_name) (DWORD, LPSTR) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IO_create_channel =
			rContext->psFunctions.psFunctions_v1.pvfCreateChannel;
	else
		if (rContext->dwVersion == IFD_HVERSION_2_0)
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
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		rv = (*IO_create_channel) (rContext->dwPort);
	} else if (rContext->dwVersion == IFD_HVERSION_2_0)
	{
		rv = (*IFDH_create_channel) (rContext->dwSlot, rContext->dwPort);
	} else
	{
		/* use device name only if defined */
		if (rContext->lpcDevice[0] != '\0')
			rv = (*IFDH_create_channel_by_name) (rContext->dwSlot, rContext->lpcDevice);
		else
			rv = (*IFDH_create_channel) (rContext->dwSlot, rContext->dwPort);
	}
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		rv = IO_Create_Channel(rContext->dwPort);
	} else if (rContext->dwVersion == IFD_HVERSION_2_0)
	{
		rv = IFDHCreateChannel(rContext->dwSlot, rContext->dwPort);
	} else
	{
		/* Use device name only if defined */
		if (rContext->lpcDevice[0] != '\0')
			rv = IFDHCreateChannelByName(rContext->dwSlot, rContext->lpcDevice);
		else
			rv = IFDHCreateChannel(rContext->dwSlot, rContext->dwPort);
	}
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

	return rv;
}

/**
 * Close a communication channel to the IFD.
 */
LONG IFDCloseIFD(PREADER_CONTEXT rContext)
{
	RESPONSECODE rv = IFD_SUCCESS;
	int repeat;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IO_close_channel) (void) = NULL;
	RESPONSECODE(*IFDH_close_channel) (DWORD) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IO_close_channel = rContext->psFunctions.psFunctions_v1.pvfCloseChannel;
	else
		IFDH_close_channel = rContext->psFunctions.psFunctions_v2.pvfCloseChannel;
#endif

	/* TRY TO LOCK THIS CODE REGION */
	repeat = 5;
again:
	rv = SYS_MutexTryLock(rContext->mMutex);
	if (EBUSY == rv)
	{
		Log1(PCSC_LOG_ERROR, "Locking failed");
		repeat--;
		if (repeat)
		{
			(void)SYS_USleep(100*1000);	/* 100 ms */
			goto again;
		}
	}

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)

		rv = (*IO_close_channel) ();
	else
		rv = (*IFDH_close_channel) (rContext->dwSlot);
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		rv = IO_Close_Channel();
	else
		rv = IFDHCloseChannel(rContext->dwSlot);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

	return rv;
}

/**
 * Set capabilities in the reader.
 */
LONG IFDSetCapabilities(PREADER_CONTEXT rContext, DWORD dwTag,
			DWORD dwLength, PUCHAR pucValue)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_set_capabilities) (DWORD, PUCHAR) = NULL;
	RESPONSECODE(*IFDH_set_capabilities) (DWORD, DWORD, DWORD, PUCHAR) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IFD_set_capabilities = rContext->psFunctions.psFunctions_v1.pvfSetCapabilities;
	else
		IFDH_set_capabilities = rContext->psFunctions.psFunctions_v2.pvfSetCapabilities;
#endif

	/*
	 * Let the calling function lock this otherwise a deadlock will
	 * result
	 */

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		rv = (*IFD_set_capabilities) (dwTag, pucValue);
	else
		rv = (*IFDH_set_capabilities) (rContext->dwSlot, dwTag,
			dwLength, pucValue);
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		rv = IFD_Set_Capabilities(dwTag, pucValue);
	else
		rv = IFDHSetCapabilities(rContext->dwSlot, dwTag, dwLength,
			pucValue);
#endif

	return rv;
}

/**
 * Get's capabilities in the reader.
 * Other functions int this file will call
 * the driver directly to not cause a deadlock.
 */
LONG IFDGetCapabilities(PREADER_CONTEXT rContext, DWORD dwTag,
	PDWORD pdwLength, PUCHAR pucValue)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_get_capabilities) (DWORD, /*@out@*/ PUCHAR) = NULL;
	RESPONSECODE(*IFDH_get_capabilities) (DWORD, DWORD, PDWORD, /*@out@*/ PUCHAR) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IFD_get_capabilities =
			rContext->psFunctions.psFunctions_v1.pvfGetCapabilities;
	else
		IFDH_get_capabilities =
			rContext->psFunctions.psFunctions_v2.pvfGetCapabilities;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		rv = (*IFD_get_capabilities) (dwTag, pucValue);
	else
		rv = (*IFDH_get_capabilities) (rContext->dwSlot, dwTag,
			pdwLength, pucValue);
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		rv = IFD_Get_Capabilities(dwTag, pucValue);
	else
		rv = IFDHGetCapabilities(rContext->dwSlot, dwTag, pdwLength,
			pucValue);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

	return rv;
}

/**
 * Power up/down or reset's an ICC located in the IFD.
 */
LONG IFDPowerICC(PREADER_CONTEXT rContext, DWORD dwAction,
	PUCHAR pucAtr, PDWORD pdwAtrLen)
{
	RESPONSECODE rv;
	short ret;
	SMARTCARD_EXTENSION sSmartCard;
	DWORD dwStatus;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_power_icc) (DWORD) = NULL;
	RESPONSECODE(*IFDH_power_icc) (DWORD, DWORD, PUCHAR, PDWORD) = NULL;
#endif

	/*
	 * Zero out everything
	 */
	rv = IFD_SUCCESS;
	dwStatus = 0;
	ucValue[0] = 0;

	/*
	 * Check that the card is inserted first
	 */
	(void)IFDStatusICC(rContext, &dwStatus, pucAtr, pdwAtrLen);

	if (dwStatus & SCARD_ABSENT)
		return SCARD_W_REMOVED_CARD;
#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IFD_power_icc = rContext->psFunctions.psFunctions_v1.pvfPowerICC;
	else
		IFDH_power_icc = rContext->psFunctions.psFunctions_v2.pvfPowerICC;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_power_icc) (dwAction);
	}
	else
	{
		rv = (*IFDH_power_icc) (rContext->dwSlot, dwAction,
			pucAtr, pdwAtrLen);

		ret = ATRDecodeAtr(&sSmartCard, pucAtr, *pdwAtrLen);
	}
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Power_ICC(dwAction);
	}
	else
		rv = IFDHPowerICC(rContext->dwSlot, dwAction, pucAtr, pdwAtrLen);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

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

	/*
	 * Get the ATR and it's length
	 */
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		(void)IFDStatusICC(rContext, &dwStatus, pucAtr, pdwAtrLen);

	return rv;
}

/**
 * Provide statistical information about the IFD and ICC including insertions,
 * atr, powering status/etc.
 */
LONG IFDStatusICC(PREADER_CONTEXT rContext, PDWORD pdwStatus,
	PUCHAR pucAtr, PDWORD pdwAtrLen)
{
	RESPONSECODE rv = IFD_SUCCESS;
	DWORD dwTag = 0, dwCardStatus = 0;
	SMARTCARD_EXTENSION sSmartCard;
	UCHAR ucValue[1] = "\x00";

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_is_icc_present) (void) = NULL;
	RESPONSECODE(*IFDH_icc_presence) (DWORD) = NULL;
	RESPONSECODE(*IFD_get_capabilities) (DWORD, /*@out@*/ PUCHAR) = NULL;

	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		IFD_is_icc_present =
			rContext->psFunctions.psFunctions_v1.pvfICCPresence;
		IFD_get_capabilities =
			rContext->psFunctions.psFunctions_v1.pvfGetCapabilities;
	}
	else
		IFDH_icc_presence = rContext->psFunctions.psFunctions_v2.pvfICCPresence;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_is_icc_present) ();
	}
	else
		rv = (*IFDH_icc_presence) (rContext->dwSlot);
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Is_ICC_Present();
	}
	else
		rv = IFDHICCPresence(rContext->dwSlot);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

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

	/*
	 * Now lets get the ATR and process it if IFD Handler version 1.0.
	 * IFD Handler version 2.0 does this immediately after reset/power up
	 * to conserve resources
	 */

	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		if (rv == IFD_SUCCESS || rv == IFD_ICC_PRESENT)
		{
			short ret;

			dwTag = TAG_IFD_ATR;

			/* LOCK THIS CODE REGION */
			(void)SYS_MutexLock(rContext->mMutex);

			ucValue[0] = rContext->dwSlot;
			(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);

#ifndef PCSCLITE_STATIC_DRIVER
			rv = (*IFD_get_capabilities) (dwTag, pucAtr);
#else
			rv = IFD_Get_Capabilities(dwTag, pucAtr);
#endif

			/* END OF LOCKED REGION */
			(void)SYS_MutexUnLock(rContext->mMutex);

			/*
			 * FIX :: This is a temporary way to return the correct size
			 * of the ATR since most of the drivers return MAX_ATR_SIZE
			 */

			ret = ATRDecodeAtr(&sSmartCard, pucAtr, MAX_ATR_SIZE);

			/*
			 * Might be a memory card without an ATR
			 */
			if (ret == 0)
				*pdwAtrLen = 0;
			else
				*pdwAtrLen = sSmartCard.ATR.Length;
		}
		else
		{
			/*
			 * No card is inserted - Atr length is 0
			 */
			*pdwAtrLen = 0;
		}
		/*
		 * End of FIX
		 */
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

LONG IFDControl_v2(PREADER_CONTEXT rContext, PUCHAR TxBuffer,
	DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_control_v2) (DWORD, PUCHAR, DWORD, /*@out@*/ PUCHAR,
		PDWORD);
#endif

	if (rContext->dwVersion != IFD_HVERSION_2_0)
		return SCARD_E_UNSUPPORTED_FEATURE;

#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_control_v2 = rContext->psFunctions.psFunctions_v2.pvfControl;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_control_v2) (rContext->dwSlot, TxBuffer, TxLength,
		RxBuffer, RxLength);
#else
	rv = IFDHControl_v2(rContext->dwSlot, TxBuffer, TxLength,
		RxBuffer, RxLength);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

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

LONG IFDControl(PREADER_CONTEXT rContext, DWORD ControlCode,
	LPCVOID TxBuffer, DWORD TxLength, LPVOID RxBuffer, DWORD RxLength,
	LPDWORD BytesReturned)
{
	RESPONSECODE rv = IFD_SUCCESS;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_control) (DWORD, DWORD, LPCVOID, DWORD, LPVOID, DWORD, LPDWORD);
#endif

	if (rContext->dwVersion < IFD_HVERSION_3_0)
		return SCARD_E_UNSUPPORTED_FEATURE;

#ifndef PCSCLITE_STATIC_DRIVER
	IFDH_control = rContext->psFunctions.psFunctions_v3.pvfControl;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	rv = (*IFDH_control) (rContext->dwSlot, ControlCode, TxBuffer,
		TxLength, RxBuffer, RxLength, BytesReturned);
#else
	rv = IFDHControl(rContext->dwSlot, ControlCode, TxBuffer,
		TxLength, RxBuffer, RxLength, BytesReturned);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

	if (rv == IFD_SUCCESS)
		return SCARD_S_SUCCESS;
	else
	{
		Log2(PCSC_LOG_ERROR, "Card not transacted: %ld", rv);
		Log3(PCSC_LOG_DEBUG, "ControlCode: 0x%.8LX BytesReturned: %ld",
			ControlCode, *BytesReturned);
		LogXxd(PCSC_LOG_DEBUG, "TxBuffer ", TxBuffer, TxLength);
		LogXxd(PCSC_LOG_DEBUG, "RxBuffer ", RxBuffer, *BytesReturned);

		if (rv == IFD_NO_SUCH_DEVICE)
		{
			(void)SendHotplugSignal();
			return SCARD_E_READER_UNAVAILABLE;
		}

		return SCARD_E_NOT_TRANSACTED;
	}
}

/**
 * Transmit an APDU to the ICC.
 */
LONG IFDTransmit(PREADER_CONTEXT rContext, SCARD_IO_HEADER pioTxPci,
	PUCHAR pucTxBuffer, DWORD dwTxLength, PUCHAR pucRxBuffer,
	PDWORD pdwRxLength, PSCARD_IO_HEADER pioRxPci)
{
	RESPONSECODE rv = IFD_SUCCESS;
	UCHAR ucValue[1] = "\x00";

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_transmit_to_icc) (SCARD_IO_HEADER, PUCHAR, DWORD,
		/*@out@*/ PUCHAR, PDWORD, PSCARD_IO_HEADER) = NULL;
	RESPONSECODE(*IFDH_transmit_to_icc) (DWORD, SCARD_IO_HEADER, PUCHAR,
		DWORD, /*@out@*/ PUCHAR, PDWORD, PSCARD_IO_HEADER) = NULL;
#endif

	/* log the APDU */
	DebugLogCategory(DEBUG_CATEGORY_APDU, pucTxBuffer, dwTxLength);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
		IFD_transmit_to_icc =
			rContext->psFunctions.psFunctions_v1.pvfTransmitToICC;
	else
		IFDH_transmit_to_icc =
			rContext->psFunctions.psFunctions_v2.pvfTransmitToICC;
#endif

	/* LOCK THIS CODE REGION */
	(void)SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_transmit_to_icc) (pioTxPci, (LPBYTE) pucTxBuffer,
			dwTxLength, pucRxBuffer, pdwRxLength, pioRxPci);
	}
	else
		rv = (*IFDH_transmit_to_icc) (rContext->dwSlot, pioTxPci,
			(LPBYTE) pucTxBuffer, dwTxLength,
			pucRxBuffer, pdwRxLength, pioRxPci);
#else
	if (rContext->dwVersion == IFD_HVERSION_1_0)
	{
		ucValue[0] = rContext->dwSlot;
		(void)IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Transmit_to_ICC(pioTxPci, (LPBYTE) pucTxBuffer,
			dwTxLength, pucRxBuffer, pdwRxLength, pioRxPci);
	}
	else
		rv = IFDHTransmitToICC(rContext->dwSlot, pioTxPci,
			(LPBYTE) pucTxBuffer, dwTxLength,
			pucRxBuffer, pdwRxLength, pioRxPci);
#endif

	/* END OF LOCKED REGION */
	(void)SYS_MutexUnLock(rContext->mMutex);

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

		return SCARD_E_NOT_TRANSACTED;
	}
}

