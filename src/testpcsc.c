
/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : test.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This is a test program for pcsc-lite.
	            
********************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "pcsclite.h"
#include "winscard.h"

/*
 * #define REPEAT_TEST 1 
 */

int main(int argc, char **argv)
{
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	SCARD_READERSTATE_A rgReaderStates[1];
	unsigned long dwReaderLen, dwState, dwProt, dwAtrLen;
	// unsigned long dwSendLength, dwRecvLength;
	unsigned long dwPref, dwReaders;
	char *pcReaders, *mszReaders;
	unsigned char pbAtr[MAX_ATR_SIZE];
	const char *mszGroups;
	long rv;
	int i, p, iReader;
	int iList[16];

	// int t = 0;

	printf("\nMUSCLE PC/SC Lite Test Program\n\n");

	printf("Testing SCardEstablishContext    : ");
	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		return -1;
	}

	printf("Testing SCardGetStatusChange \n");
	printf("Please insert a working reader   : ");
	rv = SCardGetStatusChange(hContext, INFINITE, 0, 0);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardListReaders         : ");

	mszGroups = 0;
	rv = SCardListReaders(hContext, mszGroups, 0, &dwReaders);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	mszReaders = (char *) malloc(sizeof(char) * dwReaders);
	rv = SCardListReaders(hContext, mszGroups, mszReaders, &dwReaders);

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	/*
	 * Have to understand the multi-string here 
	 */
	p = 0;
	for (i = 0; i < dwReaders - 1; i++)
	{
		++p;
		printf("Reader %02d: %s\n", p, &mszReaders[i]);
		iList[p] = i;
		while (mszReaders[++i] != 0) ;
	}

#ifdef REPEAT_TEST
	if (t == 0)
	{
#endif

		do
		{
			printf("Enter the reader number          : ");
			scanf("%d", &iReader);
			printf("\n");

			if (iReader > p || iReader <= 0)
			{
				printf("Invalid Value - try again\n");
			}
		}
		while (iReader > p || iReader <= 0);

#ifdef REPEAT_TEST
		t = 1;
	}
#endif

	rgReaderStates[0].szReader = &mszReaders[iList[iReader]];
	rgReaderStates[0].dwCurrentState = SCARD_STATE_EMPTY;

	printf("Waiting for card insertion         \n");
	rv = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);

	printf("                                 : %s\n",
		pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardConnect             : ");
	rv = SCardConnect(hContext, &mszReaders[iList[iReader]],
		SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&hCard, &dwPref);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardStatus              : ");

	dwReaderLen = 50;
	pcReaders = (char *) malloc(sizeof(char) * 50);

	rv = SCardStatus(hCard, pcReaders, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);

	printf("%s\n", pcsc_stringify_error(rv));

	printf("Current Reader Name              : %s\n", pcReaders);
	printf("Current Reader State             : %lx\n", dwState);
	printf("Current Reader Protocol          : %lx\n", dwProt - 1);
	printf("Current Reader ATR Size          : %lx\n", dwAtrLen);
	printf("Current Reader ATR Value         : ");

	for (i = 0; i < dwAtrLen; i++)
	{
		printf("%02X ", pbAtr[i]);
	}
	printf("\n");

	if (rv != SCARD_S_SUCCESS)
	{
		SCardDisconnect(hCard, SCARD_RESET_CARD);
		SCardReleaseContext(hContext);
	}

	printf("Testing SCardDisconnect          : ");
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		SCardReleaseContext(hContext);
		return -1;
	}

	printf("Testing SCardReleaseContext      : ");
	rv = SCardReleaseContext(hContext);

	printf("%s\n", pcsc_stringify_error(rv));

	if (rv != SCARD_S_SUCCESS)
	{
		return -1;
	}

	printf("\n");
	printf("PC/SC Test Completed Successfully !\n");

	return 0;
}
