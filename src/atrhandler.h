/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2002
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This keeps track of smartcard protocols, timing issues
 * and Answer to Reset ATR handling.
 */

#ifndef __atrhandler_h__
#define __atrhandler_h__

#define SCARD_CONVENTION_DIRECT  0x0001
#define SCARD_CONVENTION_INVERSE 0x0002

	typedef struct
	{

		struct
		{
			int Length;
			int HistoryLength;
			UCHAR Value[MAX_ATR_SIZE];
			UCHAR HistoryValue[MAX_ATR_SIZE];
		}
		ATR;

		struct
		{
			UCHAR AvailableProtocols;
			UCHAR CurrentProtocol;
			UCHAR Convention;
		}
		CardCapabilities;
	}
	SMARTCARD_EXTENSION;

	/*
	 * Decodes the ATR and fills the structure
	 */

	short ATRDecodeAtr(/*@out@*/ SMARTCARD_EXTENSION *psExtension,
		PUCHAR pucAtr, DWORD dwLength);

#endif							/* __atrhandler_h__ */
