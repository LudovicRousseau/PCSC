/******************************************************************

            Title  : ifdwrapper.c
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/08/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This wraps the dynamic ifdhandler functions.

********************************************************************/

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include "ifdhandler.h"
#include "ifdwrapper.h"
#include "atrhandler.h"
#include "dyn_generic.h"
#include "sys_generic.h"

/*
 * Function: IFDSetPTS Purpose : To set the protocol type selection (PTS). 
 * This function sets the appropriate protocol to be used on the card. 
 */

LONG IFDSetPTS(PREADER_CONTEXT rContext, DWORD dwProtocol, UCHAR ucFlags,
	UCHAR ucPTS1, UCHAR ucPTS2, UCHAR ucPTS3)
{

	RESPONSECODE rv;
	LPVOID vFunction;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_set_protocol_parameters) (DWORD, UCHAR, UCHAR,
		UCHAR, UCHAR) = NULL;
	RESPONSECODE(*IFDH_set_protocol_parameters) (DWORD, DWORD, UCHAR,
		UCHAR, UCHAR, UCHAR) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;
	ucValue[0] = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfSetProtocol;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_set_protocol_parameters = (RESPONSECODE(*)(DWORD, UCHAR, 
							       UCHAR, UCHAR, 
							       UCHAR)) 
		  vFunction;
	} else
	{
		IFDH_set_protocol_parameters =
			(RESPONSECODE(*)(DWORD, DWORD, UCHAR, UCHAR, UCHAR,
				UCHAR)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */
	SYS_MutexLock(rContext->mMutex);

	ucValue[0] = rContext->dwSlot;

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
	        rv = (*IFD_set_protocol_parameters) (dwProtocol,
			ucFlags, ucPTS1, ucPTS2, ucPTS3);
	} else
	{
		rv = (*IFDH_set_protocol_parameters) (rContext->dwSlot, 
						      dwProtocol,
						      ucFlags, ucPTS1, 
						      ucPTS2, ucPTS3);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Set_Protocol_Parameters(dwProtocol, ucFlags, ucPTS1,
			ucPTS2, ucPTS3);
	} else
	{
		rv = IFDHSetProtocolParameters(rContext->dwSlot, dwProtocol,
			ucFlags, ucPTS1, ucPTS2, ucPTS3);
	}
#endif

	SYS_MutexUnLock(rContext->mMutex);
	/*
	 * END OF LOCKED REGION 
	 */

	return rv;
}

/*
 * Function: IFDOpenIFD Purpose : This function opens a communication
 * channel to the IFD. 
 */

LONG IFDOpenIFD(PREADER_CONTEXT rContext, DWORD dwChannelId)
{

	RESPONSECODE rv;
	LPVOID vFunction;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IO_create_channel) (DWORD) = NULL;
	RESPONSECODE(*IFDH_create_channel) (DWORD, DWORD) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfCreateChannel;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IO_create_channel = (RESPONSECODE(*)(DWORD)) vFunction;
	} else
	{
		IFDH_create_channel = (RESPONSECODE(*)(DWORD, DWORD)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);
#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = (*IO_create_channel) (dwChannelId);
	} else
	{
		rv = (*IFDH_create_channel) (rContext->dwSlot, dwChannelId);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = IO_Create_Channel(dwChannelId);
	} else
	{
		rv = IFDHCreateChannel(rContext->dwSlot, dwChannelId);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	return rv;
}

/*
 * Function: IFDCloseIFD Purpose : This function closes a communication
 * channel to the IFD. 
 */

LONG IFDCloseIFD(PREADER_CONTEXT rContext)
{

	RESPONSECODE rv;
	LPVOID vFunction;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IO_close_channel) () = NULL;
	RESPONSECODE(*IFDH_close_channel) (DWORD) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfCloseChannel;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IO_close_channel = (RESPONSECODE(*)())vFunction;
	} else
	{
		IFDH_close_channel = (RESPONSECODE(*)(DWORD)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);
#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = (*IO_close_channel) ();
	} else
	{
		rv = (*IFDH_close_channel) (rContext->dwSlot);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = IO_Close_Channel();
	} else
	{
		rv = IFDHCloseChannel(rContext->dwSlot);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	return rv;
}

/*
 * Function: IFDSetCapabilites Purpose : This function set's capabilities
 * in the reader. 
 */

LONG IFDSetCapabilities(PREADER_CONTEXT rContext, DWORD dwTag,
			DWORD dwLength, PUCHAR pucValue)
{

	LONG rv;
	LPVOID vFunction;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_set_capabilities) (DWORD, PUCHAR) = NULL;
	RESPONSECODE(*IFDH_set_capabilities) (DWORD, DWORD, DWORD, PUCHAR) =
		NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfSetCapabilities;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_set_capabilities = (RESPONSECODE(*)(DWORD, PUCHAR)) 
		  vFunction;
	} else
	{
		IFDH_set_capabilities = (RESPONSECODE(*)(DWORD, DWORD, DWORD,
				PUCHAR)) vFunction;
	}
#endif

	/*
	 * Let the calling function lock this otherwise a deadlock will
	 * result 
	 */

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = (*IFD_set_capabilities) (dwTag, pucValue);
	} else
	{
		rv = (*IFDH_set_capabilities) (rContext->dwSlot, dwTag,
			dwLength, pucValue);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = IFD_Set_Capabilities(dwTag, pucValue);
	} else
	{
		rv = IFDHSetCapabilities(rContext->dwSlot, dwTag, dwLength,
			pucValue);
	}
#endif

	return rv;
}

/*
 * Function: IFDGetCapabilites Purpose : This function get's capabilities
 * in the reader. Other functions int this file will call the driver
 * directly to not cause a deadlock. 
 */

LONG IFDGetCapabilities(PREADER_CONTEXT rContext, DWORD dwTag,
	PDWORD pdwLength, PUCHAR pucValue)
{

	LONG rv;
	LPVOID vFunction;

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_get_capabilities) (DWORD, PUCHAR) = NULL;
	RESPONSECODE(*IFDH_get_capabilities) (DWORD, DWORD, PDWORD, PUCHAR) =
		NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfGetCapabilities;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_get_capabilities = (RESPONSECODE(*)(DWORD, PUCHAR)) 
		  vFunction;
	} else
	{
		IFDH_get_capabilities = (RESPONSECODE(*)(DWORD, DWORD, PDWORD,
				PUCHAR)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = (*IFD_get_capabilities) (dwTag, pucValue);
	} else
	{
		rv = (*IFDH_get_capabilities) (rContext->dwSlot, dwTag,
			pdwLength, pucValue);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = IFD_Get_Capabilities(dwTag, pucValue);
	} else
	{
		rv = IFDHGetCapabilities(rContext->dwSlot, dwTag, pdwLength,
			pucValue);
	}
#endif

	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	return rv;
}

/*
 * Function: IFDPowerICC Purpose : This function powers up/down or reset's 
 * an ICC located in the IFD. 
 */

LONG IFDPowerICC(PREADER_CONTEXT rContext, DWORD dwAction,
	PUCHAR pucAtr, PDWORD pdwAtrLen)
{

	RESPONSECODE rv, ret;
	LPVOID vFunction;
	SMARTCARD_EXTENSION sSmartCard;
	DWORD dwStatus, dwProtocol;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_power_icc) (DWORD) = NULL;
	RESPONSECODE(*IFDH_power_icc) (DWORD, DWORD, PUCHAR, PDWORD) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;
	dwStatus = 0;
	dwProtocol = 0;
	ucValue[0] = 0;

	/*
	 * Check that the card is inserted first 
	 */
	IFDStatusICC(rContext, &dwStatus, &dwProtocol, pucAtr, pdwAtrLen);

	if (dwStatus & SCARD_ABSENT)
	{
		return SCARD_W_REMOVED_CARD;
	}
#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfPowerICC;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_power_icc = (RESPONSECODE(*)(DWORD)) vFunction;
	} else
	{
		IFDH_power_icc = (RESPONSECODE(*)(DWORD, DWORD, PUCHAR,
				PDWORD)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_power_icc) (dwAction);
	} else
	{
		rv = (*IFDH_power_icc) (rContext->dwSlot, dwAction,
			pucAtr, pdwAtrLen);

		ret = ATRDecodeAtr(&sSmartCard, pucAtr, *pdwAtrLen);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Power_ICC(dwAction);
	} else
	{
		rv = IFDHPowerICC(rContext->dwSlot, dwAction, pucAtr, dwAtrLen);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	/*
	 * Get the ATR and it's length 
	 */
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFDStatusICC(rContext, &dwStatus, &dwProtocol, pucAtr, pdwAtrLen);
	}

	return rv;
}

/*
 * Function: IFDStatusICC Purpose : This function provides statistical
 * information about the IFD and ICC including insertions, atr, powering
 * status/etc. 
 */

LONG IFDStatusICC(PREADER_CONTEXT rContext, PDWORD pdwStatus,
	PDWORD pdwProtocol, PUCHAR pucAtr, PDWORD pdwAtrLen)
{

	RESPONSECODE rv, rv1;
	LPVOID vFunctionA, vFunctionB;
	DWORD dwTag, dwCardStatus;
	SMARTCARD_EXTENSION sSmartCard;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_is_icc_present) () = NULL;
	RESPONSECODE(*IFDH_icc_presence) (DWORD) = NULL;
	RESPONSECODE(*IFD_get_capabilities) (DWORD, PUCHAR) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	rv1 = 0;
	vFunctionA = 0;
	vFunctionB = 0;
	dwTag = 0;
	dwCardStatus = 0;
	ucValue[0] = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunctionA = rContext->psFunctions.pvfICCPresent;
	vFunctionB = rContext->psFunctions.pvfGetCapabilities;

	if (vFunctionA == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (vFunctionB == 0 && rContext->dwVersion & IFD_HVERSION_1_0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_is_icc_present = (RESPONSECODE(*)())vFunctionA;
		IFD_get_capabilities = (RESPONSECODE(*)(DWORD, PUCHAR)) 
		  vFunctionB;
	} else
	{
		IFDH_icc_presence = (RESPONSECODE(*)(DWORD)) vFunctionA;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_is_icc_present) ();
	} else
	{
		rv = (*IFDH_icc_presence) (rContext->dwSlot);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Is_ICC_Present();
	} else
	{
		rv = IFDHICCPresence(rContext->dwSlot);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	if (rv == IFD_SUCCESS || rv == IFD_ICC_PRESENT)
	{
		dwCardStatus |= SCARD_PRESENT;
	} else if (rv == IFD_ICC_NOT_PRESENT)
	{
		dwCardStatus |= SCARD_ABSENT;
	} else
	{
		return SCARD_E_NOT_TRANSACTED;
	}

	/*
	 * Now lets get the ATR and process it if IFD Handler version 1.0 2.0
	 * does this immediately after reset/power up to conserve resources 
	 */

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		if (rv == IFD_SUCCESS || rv == IFD_ICC_PRESENT)
		{
			dwTag = TAG_IFD_ATR;

			/*
			 * LOCK THIS CODE REGION 
			 */

			SYS_MutexLock(rContext->mMutex);

			ucValue[0] = rContext->dwSlot;
			IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);

#ifndef PCSCLITE_STATIC_DRIVER
			rv = (*IFD_get_capabilities) (dwTag, pucAtr);
#else
			rv = IFD_Get_Capabilities(dwTag, pucAtr);
#endif
			SYS_MutexUnLock(rContext->mMutex);

			/*
			 * END OF LOCKED REGION 
			 */

			/*
			 * FIX :: This is a temporary way to return the correct size
			 * of the ATR since most of the drivers return MAX_ATR_SIZE 
			 */

			rv = ATRDecodeAtr(&sSmartCard, pucAtr, MAX_ATR_SIZE);

			/*
			 * Might be a memory card without an ATR 
			 */
			if (rv == 0)
			{
				*pdwAtrLen = 0;
			} else
			{
				*pdwAtrLen = sSmartCard.ATR.Length;
			}
		} else
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
	*pdwProtocol = rContext->dwProtocol;

	return SCARD_S_SUCCESS;
}

/*
 * Function: IFDControl Purpose : This function provides a means for
 * toggling a specific action on the reader such as swallow, eject,
 * biometric. 
 */

LONG IFDControl(PREADER_CONTEXT rContext, PUCHAR TxBuffer,
	DWORD TxLength, PUCHAR RxBuffer, PDWORD RxLength)
{

	RESPONSECODE rv;
	LPVOID vFunction;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFDH_control) (DWORD, PUCHAR, DWORD, PUCHAR, PDWORD);
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;
	ucValue[0] = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfControl;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	IFDH_control = (RESPONSECODE(*)(DWORD, PUCHAR, DWORD,
			PUCHAR, PDWORD)) vFunction;
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);

#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	} else
	{
		rv = (*IFDH_control) (rContext->dwSlot, TxBuffer, TxLength,
			RxBuffer, RxLength);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		rv = SCARD_E_UNSUPPORTED_FEATURE;
	} else
	{
		rv = IFDHControl(rContext->dwSlot, TxBuffer, TxLength,
			RxBuffer, RxLength);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	if (rv == IFD_SUCCESS)
	{
		return SCARD_S_SUCCESS;
	} else
	{
		return SCARD_E_NOT_TRANSACTED;
	}
}

/*
 * Function: IFDTransmit Purpose : This function transmits an APDU to the
 * ICC. 
 */

LONG IFDTransmit(PREADER_CONTEXT rContext, SCARD_IO_HEADER pioTxPci,
	PUCHAR pucTxBuffer, DWORD dwTxLength, PUCHAR pucRxBuffer,
	PDWORD pdwRxLength, PSCARD_IO_HEADER pioRxPci)
{

	RESPONSECODE rv;
	LPVOID vFunction;
	UCHAR ucValue[1];

#ifndef PCSCLITE_STATIC_DRIVER
	RESPONSECODE(*IFD_transmit_to_icc) (SCARD_IO_HEADER, PUCHAR, DWORD,
		PUCHAR, DWORD *, PSCARD_IO_HEADER) = NULL;
	RESPONSECODE(*IFDH_transmit_to_icc) (DWORD, SCARD_IO_HEADER, PUCHAR,
		DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER) = NULL;
#endif

	/*
	 * Zero out everything 
	 */
	rv = 0;
	vFunction = 0;
	ucValue[0] = 0;

#ifndef PCSCLITE_STATIC_DRIVER
	/*
	 * Make sure the symbol exists in the driver 
	 */
	vFunction = rContext->psFunctions.pvfTransmitICC;

	if (vFunction == 0)
	{
		return SCARD_E_UNSUPPORTED_FEATURE;
	}

	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
		IFD_transmit_to_icc = (RESPONSECODE(*)(SCARD_IO_HEADER, PUCHAR,
						       DWORD, PUCHAR, DWORD *,
						       PSCARD_IO_HEADER)) 
		  vFunction;
	} else
	{
		IFDH_transmit_to_icc =
			(RESPONSECODE(*)(DWORD, SCARD_IO_HEADER, PUCHAR, 
					 DWORD, PUCHAR, DWORD *, 
					 PSCARD_IO_HEADER)) vFunction;
	}
#endif

	/*
	 * LOCK THIS CODE REGION 
	 */

	SYS_MutexLock(rContext->mMutex);


#ifndef PCSCLITE_STATIC_DRIVER
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = (*IFD_transmit_to_icc) (pioTxPci, (LPBYTE) pucTxBuffer,
			dwTxLength, pucRxBuffer, pdwRxLength, pioRxPci);
	} else
	{
		rv = (*IFDH_transmit_to_icc) (rContext->dwSlot, pioTxPci,
			(LPBYTE) pucTxBuffer, dwTxLength,
			pucRxBuffer, pdwRxLength, pioRxPci);
	}
#else
	if (rContext->dwVersion & IFD_HVERSION_1_0)
	{
	        ucValue[0] = rContext->dwSlot;
	        IFDSetCapabilities(rContext, TAG_IFD_SLOTNUM, 1, ucValue);
		rv = IFD_Transmit_to_ICC(pioTxPci, (LPBYTE) pucTxBuffer,
			dwTxLength, pucRxBuffer, pdwRxLength, pioRxPci);
	} else
	{
		rv = IFDHTransmitToICC(rContext->dwSlot, pioTxPci,
			(LPBYTE) pucTxBuffer, dwTxLength,
			pucRxBuffer, pdwRxLength, pioRxPci);
	}
#endif
	SYS_MutexUnLock(rContext->mMutex);

	/*
	 * END OF LOCKED REGION 
	 */

	if (rv == IFD_SUCCESS)
	{
		return SCARD_S_SUCCESS;
	} else
	{
		return SCARD_E_NOT_TRANSACTED;
	}

}
