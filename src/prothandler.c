/******************************************************************

            Title  : prothandler.c
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/06/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles protocol defaults, PTS, etc.

********************************************************************/

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include "prothandler.h"
#include "atrhandler.h"
#include "ifdhandler.h"
#include "ifdwrapper.h"
#include "debuglog.h"

/*
 * Function: PHGetDefaultProtocol Purpose : To get the default protocol
 * used immediately after reset. This protocol is returned from the
 * function. 
 */

UCHAR PHGetDefaultProtocol(PUCHAR pucAtr, DWORD dwLength)
{

	SMARTCARD_EXTENSION sSmartCard;

	/*
	 * Zero out everything 
	 */
	memset(&sSmartCard, 0x00, sizeof(SMARTCARD_EXTENSION));

	if (ATRDecodeAtr(&sSmartCard, pucAtr, dwLength))
	{
		return sSmartCard.CardCapabilities.CurrentProtocol;
	} else
	{
		return 0x00;
	}

}

/*
 * Function: PHGetAvailableProtocols Purpose : To get the protocols
 * supported by the card. These protocols are returned from the function
 * as bit masks. 
 */

UCHAR PHGetAvailableProtocols(PUCHAR pucAtr, DWORD dwLength)
{

	SMARTCARD_EXTENSION sSmartCard;

	/*
	 * Zero out everything 
	 */
	memset(&sSmartCard, 0x00, sizeof(SMARTCARD_EXTENSION));

	if (ATRDecodeAtr(&sSmartCard, pucAtr, dwLength))
	{
		return sSmartCard.CardCapabilities.AvailableProtocols;
	} else
	{
		return 0x00;
	}

}

/*
 * Function: PHSetProtocol Purpose : To determine which protocol to use.
 * SCardConnect has a DWORD dwPreferredProtocols that is a bitmask of what 
 * protocols to use.  Basically, if T=N where N is not zero will be used
 * first if it is available in ucAvailable.  Otherwise it will always
 * default to T=0.  Do nothing if the default rContext->dwProtocol is OK. 
 */

UCHAR PHSetProtocol(struct ReaderContext * rContext,
	DWORD dwPreferred, UCHAR ucAvailable)
{
	LONG rv;

	/*
	 * Zero out everything 
	 */
	rv = 0;

	if (dwPreferred == 0)
	{
		/*
		 * App has specified no protocol 
		 */
		return -1;
	}

	if ((rContext->dwProtocol == SCARD_PROTOCOL_T1) &&
		((dwPreferred & SCARD_PROTOCOL_T1) == 0) &&
		(dwPreferred & SCARD_PROTOCOL_T0))
	{

		if (SCARD_PROTOCOL_T0 & ucAvailable)
		{
			DebugLogB("PHSetProtocol: Attempting PTS to T=%d", 0);

			/*
			 * Case 1: T1 is default but is not preferred 
			 */
			/*
			 * Soln : Change to T=0 protocol.  
			 */
			rv = IFDSetPTS(rContext, SCARD_PROTOCOL_T0, 0x00,
				0x00, 0x00, 0x00);

			if (rv != SCARD_S_SUCCESS)
			{
				return SCARD_PROTOCOL_T1;
			}

		} else
		{
			/*
			 * App wants an unsupported protocol 
			 */
			return -1;
		}

	} else if ((rContext->dwProtocol == SCARD_PROTOCOL_T0) &&
		((dwPreferred & SCARD_PROTOCOL_T0) == 0) &&
		(dwPreferred & SCARD_PROTOCOL_T1))
	{

		DebugLogB("PHSetProtocol: Attempting PTS to T=%d", 1);

		/*
		 * Case 2: T=0 is default but T=1 is preferred 
		 */
		/*
		 * Soln : Change to T=1 only if supported 
		 */

		if (ucAvailable & SCARD_PROTOCOL_T1)
		{
			rv = IFDSetPTS(rContext, SCARD_PROTOCOL_T1, 0x00,
				0x00, 0x00, 0x00);

			if (rv != SCARD_S_SUCCESS)
			{
				return SCARD_PROTOCOL_T0;
			} else
			{
				return SCARD_PROTOCOL_T1;
			}

		} else
		{
			/*
			 * App wants unsupported protocol 
			 */
			return -1;
		}

	} else
	{
		/*
		 * Case 3: Default protocol is preferred 
		 */
		/*
		 * Soln : No need to change protocols 
		 */
		return rContext->dwProtocol;
	}

	return rContext->dwProtocol;
}
