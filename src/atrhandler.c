/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : atrhandler.c
            Author : David Corcoran
            Date   : 7/27/99
            License: Copyright (C) 1999 David Corcoran
                     <corcoran@linuxnet.com> 
            Purpose: This keeps track of smartcard protocols,
                     timing issues, and atr handling.
 
********************************************************************/

#include <syslog.h>

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "atrhandler.h"

/*
 * Uncomment the following for ATR debugging 
 */
/*
 * #define ATR_DEBUG 1 
 */

short ATRDecodeAtr(PSMARTCARD_EXTENSION psExtension,
	PUCHAR pucAtr, DWORD dwLength)
{

	USHORT p;
	UCHAR K, TCK;				/* MSN of T0/Check Sum */
	UCHAR Y1i, T;				/* MSN/LSN of TDi */
	short TAi, TBi, TCi, TDi;	/* Interface characters */

	/*
	 * Zero out everything 
	 */
	p = K = TCK = Y1i = T = TAi = TBi = TCi = TDi = 0;

	if (dwLength < 2)
	{
		return 0;	/* Atr must have TS and T0 */
	}

	/*
	 * Zero out the bitmasks 
	 */

	psExtension->CardCapabilities.AvailableProtocols = 0x00;
	psExtension->CardCapabilities.CurrentProtocol = 0x00;

	/*
	 * Decode the TS byte 
	 */

	if (pucAtr[0] == 0x3F)
	{	/* Inverse convention used */
		psExtension->CardCapabilities.Convention =
			SCARD_CONVENTION_INVERSE;
	} else if (pucAtr[0] == 0x3B)
	{	/* Direct convention used */
		psExtension->CardCapabilities.Convention = SCARD_CONVENTION_DIRECT;
	} else
	{
		memset(psExtension, 0x00, sizeof(SMARTCARD_EXTENSION));
		return 0;
	}

	/*
	 * Here comes the platform dependant stuff 
	 */

	/*
	 * Decode the T0 byte 
	 */
	Y1i = pucAtr[1] >> 4;	/* Get the MSN in Y1 */
	K = pucAtr[1] & 0x0F;	/* Get the LSN in K */

	p = 2;

#ifdef ATR_DEBUG
	debug_msg("Conv %02X, Y1 %02X, K %02X",
		psExtension->CardCapabilities.Convention, Y1i, K);
#endif

	/*
	 * Examine Y1 
	 */

	do
	{

		TAi = (Y1i & 0x01) ? pucAtr[p++] : -1;
		TBi = (Y1i & 0x02) ? pucAtr[p++] : -1;
		TCi = (Y1i & 0x04) ? pucAtr[p++] : -1;
		TDi = (Y1i & 0x08) ? pucAtr[p++] : -1;

#ifdef ATR_DEBUG
		debug_msg("T's %02X %02X %02X %02X", TAi, TBi, TCi, TDi);
		debug_msg("P %02X", p);
#endif

		/*
		 * Examine TDi to determine protocol and more 
		 */
		if (TDi >= 0)
		{
			Y1i = TDi >> 4;	/* Get the MSN in Y1 */
			T = TDi & 0x0F;	/* Get the LSN in K */

			/*
			 * Set the current protocol TD1 
			 */
			if (psExtension->CardCapabilities.CurrentProtocol == 0x00)
			{
				switch (T)
				{
				case 0:
					psExtension->CardCapabilities.CurrentProtocol =
						SCARD_PROTOCOL_T0;
					break;
				case 1:
					psExtension->CardCapabilities.CurrentProtocol =
						SCARD_PROTOCOL_T1;
					break;
				default:
					return 0;
				}
			}

			if (T == 0)
			{
#ifdef ATR_DEBUG
				debug_msg("T=0 Protocol Found");
#endif
				psExtension->CardCapabilities.AvailableProtocols |=
					SCARD_PROTOCOL_T0;
				psExtension->CardCapabilities.T0.BGT = 0;
				psExtension->CardCapabilities.T0.BWT = 0;
				psExtension->CardCapabilities.T0.CWT = 0;
				psExtension->CardCapabilities.T0.CGT = 0;
				psExtension->CardCapabilities.T0.WT = 0;
			} else if (T == 1)
			{
#ifdef ATR_DEBUG
				debug_msg("T=1 Protocol Found");
#endif
				psExtension->CardCapabilities.AvailableProtocols |=
					SCARD_PROTOCOL_T1;
				psExtension->CardCapabilities.T1.BGT = 0;
				psExtension->CardCapabilities.T1.BWT = 0;
				psExtension->CardCapabilities.T1.CWT = 0;
				psExtension->CardCapabilities.T1.CGT = 0;
				psExtension->CardCapabilities.T1.WT = 0;
			} else
			{
				psExtension->CardCapabilities.AvailableProtocols |= T;
				/*
				 * Do nothing for now since other protocols are not
				 * supported at this time 
				 */
			}

		} else
		{
			Y1i = 0;
		}

		if (p > MAX_ATR_SIZE)
		{
			memset(psExtension, 0x00, sizeof(SMARTCARD_EXTENSION));
			return 0;
		}

	}
	while (Y1i != 0);

	/*
	 * If TDx is not set then the current must be T0 
	 */
	if (psExtension->CardCapabilities.CurrentProtocol == 0x00)
	{
		psExtension->CardCapabilities.CurrentProtocol = SCARD_PROTOCOL_T0;
		psExtension->CardCapabilities.AvailableProtocols |=
			SCARD_PROTOCOL_T0;
	}

	/*
	 * Take care of the historical characters 
	 */

	psExtension->ATR.HistoryLength = K;
	memcpy(psExtension->ATR.HistoryValue, &pucAtr[p], K);

	p = p + K;

	/*
	 * Check to see if TCK character is included It will be included if
	 * more than T=0 is supported 
	 */

	if (psExtension->CardCapabilities.AvailableProtocols &
		SCARD_PROTOCOL_T1)
	{
		TCK = pucAtr[p++];
	}

	memcpy(psExtension->ATR.Value, pucAtr, p);
	psExtension->ATR.Length = p;	/* modified from p-1 */

	return 1;
}
