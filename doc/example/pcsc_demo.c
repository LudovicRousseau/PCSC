/*
 * Sample program to use PC/SC API.
 *
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2003-2021
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include <winscard.h>

/* PCSC error message pretty print */
#define PCSC_ERROR(rv, text) \
if (rv != SCARD_S_SUCCESS) \
{ \
	printf(text ": %s (0x%lX)\n", pcsc_stringify_error(rv), rv); \
	goto end; \
} \
else \
{ \
	printf(text ": OK\n\n"); \
}

int main(int argc, char *argv[])
{
	LONG rv;
	SCARDCONTEXT hContext;
	DWORD dwReaders;
	LPSTR mszReaders = NULL;
	char *ptr, **readers = NULL;
	int nbReaders;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwReaderLen, dwState, dwProt, dwAtrLen;
	BYTE pbAtr[MAX_ATR_SIZE] = "";
	char pbReader[MAX_READERNAME] = "";
	int reader_nb;
	unsigned int i;
	const SCARD_IO_REQUEST *pioSendPci;
	SCARD_IO_REQUEST pioRecvPci;
	BYTE pbRecvBuffer[10];
	BYTE pbSendBuffer[] = { 0x00, 0xA4, 0x00, 0x00, 0x02, 0x3F, 0x00 };
	DWORD dwSendLength, dwRecvLength;

	printf("PC/SC sample code\n");
	printf("V 1.4 2003-2009, Ludovic Rousseau <ludovic.rousseau@free.fr>\n");

	printf("\nTHIS PROGRAM IS NOT DESIGNED AS A TESTING TOOL FOR END USERS!\n");
	printf("Do NOT use it unless you really know what you do.\n\n");

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %lX\n", rv);
		return EXIT_FAILURE;
	}

	/* Retrieve the available readers list. */
	dwReaders = SCARD_AUTOALLOCATE;
	rv = SCardListReaders(hContext, NULL, (LPSTR)&mszReaders, &dwReaders);
	PCSC_ERROR(rv, "SCardListReaders")

	/* Extract readers from the null separated string and get the total
	 * number of readers */
	nbReaders = 0;
	ptr = mszReaders;
	while (*ptr != '\0')
	{
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (nbReaders == 0)
	{
		printf("No reader found\n");
		goto end;
	}

	/* allocate the readers table */
	readers = calloc(nbReaders, sizeof(char *));
	if (NULL == readers)
	{
		printf("Not enough memory for readers[]\n");
		goto end;
	}

	/* fill the readers table */
	nbReaders = 0;
	ptr = mszReaders;
	while (*ptr != '\0')
	{
		printf("%d: %s\n", nbReaders, ptr);
		readers[nbReaders] = ptr;
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (argc > 1)
	{
		reader_nb = atoi(argv[1]);
		if (reader_nb < 0 || reader_nb >= nbReaders)
		{
			printf("Wrong reader index: %d\n", reader_nb);
			goto end;
		}
	}
	else
		reader_nb = 0;

	/* connect to a card */
	dwActiveProtocol = -1;
	rv = SCardConnect(hContext, readers[reader_nb], SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR(rv, "SCardConnect")

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardStatus")

    switch(dwActiveProtocol)
    {
        case SCARD_PROTOCOL_T0:
            pioSendPci = SCARD_PCI_T0;
            break;
        case SCARD_PROTOCOL_T1:
            pioSendPci = SCARD_PCI_T1;
            break;
        default:
            printf("Unknown protocol\n");
            goto end;
    }

	/* exchange APDU */
	dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	PCSC_ERROR(rv, "SCardDisconnect")

	/* connect to a card */
	dwActiveProtocol = -1;
	rv = SCardConnect(hContext, readers[reader_nb], SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, &hCard, &dwActiveProtocol);
	printf(" Protocol: %ld\n", dwActiveProtocol);
	PCSC_ERROR(rv, "SCardConnect")

	/* exchange APDU */
	dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* card reconnect */
	rv = SCardReconnect(hCard, SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD,
		&dwActiveProtocol);
	PCSC_ERROR(rv, "SCardReconnect")

	/* get card status */
	dwAtrLen = sizeof(pbAtr);
	dwReaderLen = sizeof(pbReader);
	rv = SCardStatus(hCard, /*NULL*/ pbReader, &dwReaderLen, &dwState, &dwProt,
		pbAtr, &dwAtrLen);
	printf(" Reader: %s (length %ld bytes)\n", pbReader, dwReaderLen);
	printf(" State: 0x%lX\n", dwState);
	printf(" Prot: %ld\n", dwProt);
	printf(" ATR (length %ld bytes):", dwAtrLen);
	for (i=0; i<dwAtrLen; i++)
		printf(" %02X", pbAtr[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardStatus")

	/* get card status change */
	{
		/* check only one reader */
		SCARD_READERSTATE rgReaderStates[1];

		rgReaderStates[0].szReader = pbReader;
		rgReaderStates[0].dwCurrentState = SCARD_STATE_UNAWARE;

		rv = SCardGetStatusChange(hContext, 0, rgReaderStates, 1);
		printf(" state: 0x%04lX\n", rgReaderStates[0].dwEventState);
		PCSC_ERROR(rv, "SCardGetStatusChange")
	}

	/* begin transaction */
	rv = SCardBeginTransaction(hCard);
	PCSC_ERROR(rv, "SCardBeginTransaction")

	/* exchange APDU */
	dwSendLength = sizeof(pbSendBuffer);
	dwRecvLength = sizeof(pbRecvBuffer);
	printf("Sending: ");
	for (i=0; i<dwSendLength; i++)
		printf("%02X ", pbSendBuffer[i]);
	printf("\n");
	rv = SCardTransmit(hCard, pioSendPci, pbSendBuffer, dwSendLength,
		&pioRecvPci, pbRecvBuffer, &dwRecvLength);
	printf("Received: ");
	for (i=0; i<dwRecvLength; i++)
		printf("%02X ", pbRecvBuffer[i]);
	printf("\n");
	PCSC_ERROR(rv, "SCardTransmit")

	/* end transaction */
	rv = SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
	PCSC_ERROR(rv, "SCardEndTransaction")

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
	PCSC_ERROR(rv, "SCardDisconnect")

end:
	/* free allocated memory */
	if (mszReaders)
		SCardFreeMemory(hContext, mszReaders);

	/* We try to leave things as clean as possible */
	rv = SCardReleaseContext(hContext);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardReleaseContext: %s (0x%lX)\n", pcsc_stringify_error(rv),
			rv);

	if (readers)
		free(readers);

	return EXIT_SUCCESS;
} /* main */

