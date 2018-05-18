/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 */
#include <stdio.h>
#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#define SCARD_ATTR_VALUE(Class, Tag) ((((ULONG)(Class)) << 16) | ((ULONG)(Tag)))
#define SCARD_CLASS_ICC_STATE       9   /**< ICC State specific definitions */
#define SCARD_ATTR_ATR_STRING SCARD_ATTR_VALUE(SCARD_CLASS_ICC_STATE, 0x0303) /**< Answer to reset (ATR) string. */
#else
#include <winscard.h>
#include <reader.h>
#endif

int main(void)
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	LONG rv;
	char mszReaders[1024];
	DWORD dwReaders = sizeof(mszReaders);
	unsigned char pbAtr[265];	/* 264 is OK */
	DWORD dwAtrLen;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	printf("%lX\n", rv);

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	printf("%lX\n", rv);

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard,
		&dwActiveProtocol);
	printf("%lX\n", rv);

	dwAtrLen = sizeof(pbAtr);
	rv = SCardGetAttrib(hCard, SCARD_ATTR_ATR_STRING, pbAtr, &dwAtrLen);
	printf("%lX\n", rv);

	if (SCARD_S_SUCCESS == rv)
	{
		int i;

		for (i=0; i<dwAtrLen; i++)
			printf("%02X ", pbAtr[i]);
		printf("\n");
	}

	return 0;
}
