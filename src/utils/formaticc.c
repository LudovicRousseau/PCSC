/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2000-2002
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2009
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
 * @brief This is an APDU robot for pcsc-lite.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <wintypes.h>
#include <winscard.h>
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

int main(/*@unused@*/ int argc, /*@unused@*/ char *argv[])
{
	SCARDHANDLE hCard;
	SCARDCONTEXT hContext;
	SCARD_IO_REQUEST sRecvPci;
	SCARD_READERSTATE rgReaderStates[1];
	DWORD dwSendLength, dwRecvLength, dwPref, dwReaders;
	LPSTR mszReaders = NULL;
	BYTE s[MAX_BUFFER_SIZE], r[MAX_BUFFER_SIZE];
	LPCSTR mszGroups;
	LONG rv;
	FILE *fp;
	FILE *fo;
	int i, p, iReader, cnum, iProtocol;
	int iList[16];
	char pcHost[MAXHOSTNAMELEN];
	char pcAFile[FILENAME_MAX];
	char pcOFile[FILENAME_MAX];
	char line[80];
	char *line_ptr;
	unsigned int x;

	printf("\nWinscard PC/SC Lite Test Program\n\n");

	printf("Please enter the desired host (localhost for this machine) [localhost]: ");
	(void)fgets(line, sizeof(line), stdin);
	if (line[0] == '\n')
		strncpy(pcHost, "localhost", sizeof(pcHost)-1);
	else
		strncpy(pcHost, line, sizeof(pcHost)-1);

	printf("Please input the desired transmit protocol (0/1) [0]: ");
	(void)fgets(line, sizeof(line), stdin);
	if (line[0] == '\n')
		iProtocol = 0;
	else
		(void)sscanf(line, "%d", &iProtocol);

	printf("Please input the desired input apdu file: ");
	(void)fgets(line, sizeof(line), stdin);
	(void)sscanf(line, "%s", pcAFile);

	printf("Please input the desired output apdu file: ");
	(void)fgets(line, sizeof(line), stdin);
	(void)sscanf(line, "%s", pcOFile);

	fp = fopen(pcAFile, "r");
	if (fp == NULL)
	{
		perror(pcAFile);
		return 1;
	}

	fo = fopen(pcOFile, "w");
	if (fo == NULL)
	{
		perror(pcOFile);
		(int)fclose(fp);
		return 1;
	}

	rv = SCardEstablishContext(SCARD_SCOPE_USER, pcHost, NULL, &hContext);

	if (rv != SCARD_S_SUCCESS)
	{
		printf("ERROR :: Cannot Connect to Resource Manager\n");
		(int)fclose(fp);
		(int)fclose(fo);
		return 1;
	}

	mszGroups = NULL;
	rv = SCardListReaders(hContext, mszGroups, NULL, &dwReaders);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardListReaders error line %d: %08X\n", __LINE__, rv);
		goto releasecontext;
	}
	mszReaders = malloc(sizeof(char) * dwReaders);
	rv = SCardListReaders(hContext, mszGroups, mszReaders, &dwReaders);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardListReaders error line %d: %08X\n", __LINE__, rv);
		goto releasecontext;
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

	do
	{
		printf("Enter the desired reader number: ");
		(void)fgets(line, sizeof(line), stdin);
		(void)sscanf(line, "%d", &iReader);
		printf("\n");

		if (iReader > p || iReader <= 0)
		{
			printf("Invalid Value - try again\n");
		}
	}
	while (iReader > p || iReader <= 0);

	rgReaderStates[0].szReader = &mszReaders[iList[iReader]];
	rgReaderStates[0].dwCurrentState = SCARD_STATE_EMPTY;

	printf("Please insert a smart card\n");
	rv  = SCardGetStatusChange(hContext, INFINITE, rgReaderStates, 1);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardGetStatusChange error line %d: %08X\n", __LINE__, rv);
		goto releasecontext;
	}

	rv = SCardConnect(hContext, &mszReaders[iList[iReader]],
		SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&hCard, &dwPref);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardConnect error line %d: %08X\n", __LINE__, rv);
		goto releasecontext;
	}

	/*
	 * Now lets get some work done
	 */

	rv = SCardBeginTransaction(hCard);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardBeginTransaction error line %d: %08X\n", __LINE__, rv);
		goto disconnect;
	}

	cnum = 0;

	do
	{
		cnum += 1;

		if (fgets(line, sizeof(line), fp) == NULL)
			break;

		/* comments */
		if ('#' == line[0])
		{
			printf("%s", line);
			continue;
		}

		line_ptr = line;
		if (sscanf(line_ptr, "%x", &x) == 0)
			break;
		dwSendLength = x;

		line_ptr = strchr(line_ptr, ' ');
		if (line_ptr == NULL)
			break;
		line_ptr++;

		for (i = 0; i < dwSendLength; i++)
		{
			if (sscanf(line_ptr, "%x", &x) == 0)
			{
				printf("Corrupt APDU: %s\n", line);
				goto disconnect;
				return 1;
			}
			s[i] = x;

			line_ptr = strchr(line_ptr, ' ');

			if (line_ptr == NULL)
				break;

			line_ptr++;
		}

		printf("Processing Command %03d of length %03lX: ", cnum,
			dwSendLength);
		for (i=0; i<dwSendLength; i++)
			printf("%02X ", s[i]);
		printf("\n");

		memset(r, 0x00, MAX_BUFFER_SIZE);
		dwRecvLength = MAX_BUFFER_SIZE;

		if (iProtocol == 0)
		{
			rv = SCardTransmit(hCard, SCARD_PCI_T0, s, dwSendLength,
				&sRecvPci, r, &dwRecvLength);
		}
		else
		{
			if (iProtocol == 1)
			{
				rv = SCardTransmit(hCard, SCARD_PCI_T1, s, dwSendLength,
					&sRecvPci, r, &dwRecvLength);
			}
			else
			{
				printf("Invalid Protocol\n");
				goto endtransaction;
			}
		}

		if (rv != SCARD_S_SUCCESS)
		{
			fprintf(fo, ".error 0x%08lX\n", rv);
			printf("Error: 0x%08lX\n", rv);
		}
		else
		{
			fprintf(fo, "%02ld ", dwRecvLength);
			printf("Received %ld bytes: ", dwRecvLength);

			for (i = 0; i < dwRecvLength; i++)
			{
				fprintf(fo, "%02X ", r[i]);
				printf("%02X ", r[i]);
			}

			fprintf(fo, "\n");
			printf("\n");
		}

		if (rv == SCARD_W_RESET_CARD)
		{
			rv = SCardReconnect(hCard, SCARD_SHARE_SHARED,
				SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
				SCARD_RESET_CARD, &dwPref);
			if (rv != SCARD_S_SUCCESS)
			{
				printf("SCardReconnect error line %d: %08X\n", __LINE__, rv);
				goto endtransaction;
			}
		}

	}
	while (1);

endtransaction:
	(void)SCardEndTransaction(hCard, SCARD_LEAVE_CARD);
disconnect:
	(void)SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
releasecontext:
	(void)SCardReleaseContext(hContext);
	free(mszReaders);

	(int)fclose(fp);
	(int)fclose(fo);

	return 0;
}
