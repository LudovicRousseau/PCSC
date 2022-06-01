/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 */
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif

#define GREEN "\33[32m"
#define BRIGHT_RED "\33[01;31m"
#define NORMAL "\33[0m"

int main(void)
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol;
	LONG rv;
	char mszReaders[1024];
	DWORD dwReaders = sizeof(mszReaders);
	SCARD_IO_REQUEST ioRecvPci = *SCARD_PCI_T0;	/* use a default value */
	unsigned char bSendBuffer[MAX_BUFFER_SIZE];
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	DWORD send_length, length;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	printf("SCardEstablishContext %lX\n", rv);

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	printf("SCardListReaders %lX\n", rv);

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard,
		&dwActiveProtocol);
	printf("SCardConnect %lX\n", rv);

	send_length = 5;
	/* GET RANDOM for a GPK card */
	memcpy(bSendBuffer, "\x80\x84\x00\x00\x20", send_length);

	/* expected size is 0x20 + 2 = 34 bytes */
	length = 30;
	rv = SCardTransmit(hCard, SCARD_PCI_T0, bSendBuffer, send_length,
        &ioRecvPci, bRecvBuffer, &length);
	if (SCARD_E_INSUFFICIENT_BUFFER == rv)
	{
		printf(GREEN "test PASS. Insufficient buffer is expected\n" NORMAL);
	}
	else
	{
		printf(BRIGHT_RED "test FAIL\n" NORMAL);
	}
	printf("SCardTransmit %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: %ld\n", length);
	if (SCARD_S_SUCCESS == rv)
	{
		int i;

		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf("\n");
	}

	rv = SCardTransmit(hCard, SCARD_PCI_T0, bSendBuffer, send_length,
        &ioRecvPci, bRecvBuffer, &length);
	printf("SCardTransmit %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: %ld\n", length);
	if (SCARD_S_SUCCESS == rv)
	{
		int i;

		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf("\n");
	}

	length = MAX_BUFFER_SIZE_EXTENDED +1;
	printf("length: 0x%lX\n", length);
	rv = SCardTransmit(hCard, SCARD_PCI_T0, bSendBuffer, send_length,
        &ioRecvPci, bRecvBuffer, &length);
	printf("SCardTransmit %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: 0x%lX\n", length);
	if (SCARD_E_INSUFFICIENT_BUFFER == rv)
	{
		printf(BRIGHT_RED "test FAIL\n" NORMAL);
	}

	return 0;
}
