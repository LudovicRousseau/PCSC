/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : atrhandler.h
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This keeps track of smartcard protocols,
                     timing issues, and atr handling.
 
********************************************************************/

#ifndef __atrhandler_h__
#define __atrhandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

#define SCARD_CONVENTION_DIRECT  0x0001
#define SCARD_CONVENTION_INVERSE 0x0002

	typedef struct _SMARTCARD_EXTENSION
	{

		struct _ATR
		{
			DWORD Length;
			UCHAR Value[MAX_ATR_SIZE];
			DWORD HistoryLength;
			UCHAR HistoryValue[MAX_ATR_SIZE];
		}
		ATR;

		DWORD ReadTimeout;

		struct _CardCapabilities
		{
			UCHAR AvailableProtocols;
			UCHAR CurrentProtocol;
			UCHAR Convention;
			USHORT ETU;

			struct _PtsData
			{
				UCHAR F1;
				UCHAR D1;
				UCHAR I1;
				UCHAR P1;
				UCHAR N1;
			}
			PtsData;

			struct _T1
			{
				USHORT BGT;
				USHORT BWT;
				USHORT CWT;
				USHORT CGT;
				USHORT WT;
			}
			T1;

			struct _T0
			{
				USHORT BGT;
				USHORT BWT;
				USHORT CWT;
				USHORT CGT;
				USHORT WT;
			}
			T0;

		}
		CardCapabilities;

		/*
		 * PREADER_CONNECTION psReaderConnection; 
		 */

	}
	SMARTCARD_EXTENSION, *PSMARTCARD_EXTENSION;

	/*
	 * Decodes the ATR and fills the structure 
	 */

	short ATRDecodeAtr(PSMARTCARD_EXTENSION psExtension,
		PUCHAR pucAtr, DWORD dwLength);

#ifdef __cplusplus
}
#endif

#endif							/* __smclib_h__ */
