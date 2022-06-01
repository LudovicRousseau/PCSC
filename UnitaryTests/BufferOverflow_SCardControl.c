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
#include <reader.h>
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
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	DWORD length;

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	printf("SCardEstablishContext %lX\n", rv);

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	printf("SCardListReaders %lX\n", rv);

	rv = SCardConnect(hContext, mszReaders, SCARD_SHARE_DIRECT,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard,
		&dwActiveProtocol);
	printf("SCardConnect %lX\n", rv);

	/* expected size is at least 4 bytes */
	length = 3;
	rv = SCardControl(hCard, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
        bRecvBuffer, length, &length);
	if (SCARD_E_INSUFFICIENT_BUFFER == rv)
	{
		printf(GREEN "test PASS. Insufficient buffer is expected\n" NORMAL);
	}
	else
	{
		printf(BRIGHT_RED "test FAIL\n" NORMAL);
	}
	printf("SCardControl %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: %ld\n", length);
	if (SCARD_S_SUCCESS == rv)
	{
		int i;

		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf("\n");
	}

	rv = SCardControl(hCard, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
        bRecvBuffer, sizeof bRecvBuffer, &length);
	printf("SCardControl %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: %ld\n", length);
	if (SCARD_S_SUCCESS == rv)
	{
		int i;

		for (i=0; i<length; i++)
			printf("%02X ", bRecvBuffer[i]);
		printf("\n");
	}

	rv = SCardControl(hCard, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
        bRecvBuffer, MAX_BUFFER_SIZE_EXTENDED +1, &length);
	printf("SCardControl %lX: %s\n", rv, pcsc_stringify_error(rv));
	printf("Expected length: %ld\n", length);
	if (SCARD_E_INSUFFICIENT_BUFFER == rv)
	{
		printf(BRIGHT_RED "test FAIL\n" NORMAL);
	}

	return 0;
}
