/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2011
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
 *
 * @brief This keeps track of smart card protocols, timing issues
 * and ATR (Answer-to-Reset) handling.
 *
 * @note use ./configure --enable-debugatr to enable debug messages
 * to be logged.
 */

#include "config.h"
#include <string.h>

#include "misc.h"
#include "pcsclite.h"
#include "debuglog.h"
#include "atrhandler.h"

/*
 * Uncomment the following for ATR debugging
 * or use ./configure --enable-debugatr
 */
/* #define ATR_DEBUG */

/**
 * @brief parse an ATR
 *
 * @param[out] availableProtocols available protocols
 * @param[out] currentProtocol current protocol
 * @param[in] pucAtr ATR
 * @param[in] dwLength ATR length
 * @return
 */
short ATRDecodeAtr(int *availableProtocols, int *currentProtocol,
	PUCHAR pucAtr, DWORD dwLength)
{
	USHORT p;
	UCHAR Y1i, T;				/* MSN/LSN of TDi */
	int i = 1;					/* value of the index in TAi, TBi, etc. */

#ifdef ATR_DEBUG
	if (dwLength > 0)
		LogXxd(PCSC_LOG_DEBUG, "ATR: ", pucAtr, dwLength);
#endif

	/*
	 * Zero out the bitmasks
	 */
	*availableProtocols = SCARD_PROTOCOL_UNDEFINED;
	*currentProtocol = SCARD_PROTOCOL_UNDEFINED;

	if (dwLength < 2)
		return 0;	/** @retval 0 Atr must have TS and T0 */

	/*
	 * Decode the TS byte
	 */
	if ((pucAtr[0] != 0x3F) && (pucAtr[0] != 0x3B))
		return 0;	/** @retval 0 Unable to decode TS byte */

	/*
	 * Here comes the platform dependant stuff
	 */

	/*
	 * Decode the T0 byte
	 */
	Y1i = pucAtr[1] >> 4;	/* Get the MSN in Y1 */

	p = 2;

	/*
	 * Examine Y1
	 */
	do
	{
		short TAi, TBi, TCi, TDi;	/* Interface characters */

		TAi = (Y1i & 0x01) ? pucAtr[p++] : -1;
		TBi = (Y1i & 0x02) ? pucAtr[p++] : -1;
		TCi = (Y1i & 0x04) ? pucAtr[p++] : -1;
		TDi = (Y1i & 0x08) ? pucAtr[p++] : -1;

		/* We don't use TBi and TCi but we must calculate them because
		 * of the p++ in the formulae */
		(void)TBi;
		(void)TCi;

#ifdef ATR_DEBUG
		Log9(PCSC_LOG_DEBUG,
			"TA%d: %02X, TB%d: %02X, TC%d: %02X, TD%d: %02X",
			i, TAi, i, TBi, i, TCi, i, TDi);
#endif

		/*
		 * Examine TDi to determine protocol and more
		 */
		if (TDi >= 0)
		{
			Y1i = TDi >> 4;	/* Get the MSN in Y1 */
			T = TDi & 0x0F;	/* Get the LSN in K */

			/*
			 * Set the current protocol TD1 (first TD only)
			 */
			if (*currentProtocol == SCARD_PROTOCOL_UNDEFINED)
			{
				switch (T)
				{
					case 0:
						*currentProtocol = SCARD_PROTOCOL_T0;
						break;
					case 1:
						*currentProtocol = SCARD_PROTOCOL_T1;
						break;
					default:
						return 0; /** @retval 0 Unable to decode LNS */
				}
			}

#ifdef ATR_DEBUG
			Log2(PCSC_LOG_DEBUG, "T=%d Protocol Found", T);
#endif
			if (0 == T)
			{
				*availableProtocols |= SCARD_PROTOCOL_T0;
			}
			else
				if (1 == T)
				{
					*availableProtocols |= SCARD_PROTOCOL_T1;
				}
				else
					if (15 == T)
					{
						*availableProtocols |= SCARD_PROTOCOL_T15;
					}
					else
					{
						/*
						 * Do nothing for now since other protocols are not
						 * supported at this time
						 */
					}
		}
		else
			Y1i = 0;

		/* test presence of TA2 */
		if ((2 == i) && (TAi >= 0))
		{
			T = TAi & 0x0F;
#ifdef ATR_DEBUG
			Log2(PCSC_LOG_DEBUG, "Specific mode: T=%d", T);
#endif
			switch (T)
			{
				case 0:
					*currentProtocol = *availableProtocols = SCARD_PROTOCOL_T0;
					break;

				case 1:
					*currentProtocol = *availableProtocols = SCARD_PROTOCOL_T1;
					break;

				default:
					return 0; /** @retval 0 Unable do decode T protocol */
			}
		}

		if (p > MAX_ATR_SIZE)
			return 0;	/** @retval 0 Maximum attribute size */

		/* next interface characters index */
		i++;
	}
	while (Y1i != 0);

	/*
	 * If TDx is not set then the current must be T0
	 */
	if (*currentProtocol == SCARD_PROTOCOL_UNDEFINED)
	{
		*currentProtocol = SCARD_PROTOCOL_T0;
		*availableProtocols |= SCARD_PROTOCOL_T0;
	}

#ifdef ATR_DEBUG
	Log3(PCSC_LOG_DEBUG, "CurrentProtocol: T=%d, AvailableProtocols: %d",
		*currentProtocol - SCARD_PROTOCOL_T0, *availableProtocols);
#endif

	return 1; /** @retval 1 Success */
}
