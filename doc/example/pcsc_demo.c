/*
	pcsc_demo: sample program to use PC/SC API
    Copyright (C) 2003  Ludovic Rousseau <ludovic.rousseau@free.fr>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <wintypes.h>
#include <winscard.h>

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

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
	LPSTR mszReaders;
	char *ptr, *readers[PCSCLITE_MAX_CHANNELS];
	int nbReaders;
	SCARDHANDLE hCard;
	DWORD dwActiveProtocol, dwReaderLen, dwState, dwProt, dwAtrLen;
	BYTE pbAtr[MAX_ATR_SIZE] = "";
	BYTE pbReader[MAX_READERNAME] = "";
	int i;

	printf("PC/SC sample code\n");
	printf("V 1.0 2003, Ludovic Rousseau <ludovic.rousseau@free.fr>\n");

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardEstablishContext: Cannot Connect to Resource Manager %lX\n", rv);
		return 1;
	}

	/* Retrieve the available readers list.
	 *
	 * 1. Call with a null buffer to get the number of bytes to allocate
	 * 2. malloc the necessary storage
	 * 3. call with the real allocated buffer
	 */
	rv = SCardListReaders(hContext, NULL, NULL, &dwReaders);
	if (rv != SCARD_S_SUCCESS)
	{
		printf("SCardListReader: %lX\n", rv);
	}

	mszReaders = malloc(sizeof(char)*dwReaders);
	if (mszReaders == NULL)
	{
		printf("malloc: not enough memory\n");
		goto end;
	}

	rv = SCardListReaders(hContext, NULL, mszReaders, &dwReaders);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardListReader: %lX\n", rv);

	/* Extract readers from the null separated string and get thetotal
	 * number of readers
	 */
	nbReaders = 0;
	ptr = mszReaders;
	while ((*ptr != '\0') && (nbReaders < PCSCLITE_MAX_CHANNELS))
	{
		printf("%d: %s\n", nbReaders, ptr);
		readers[nbReaders] = ptr;
		ptr += strlen(ptr)+1;
		nbReaders++;
	}

	if (nbReaders == 0)
	{
		printf("No reader found\n");
		goto end;
	}

	/* connect to a card */
	rv = SCardConnect(hContext, readers[0], SCARD_SHARE_SHARED,
		SCARD_PROTOCOL_T0, &hCard, &dwActiveProtocol);
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

	/* card disconnect */
	rv = SCardDisconnect(hCard, SCARD_UNPOWER_CARD);
	PCSC_ERROR(rv, "SCardDisconnect")

end:
	/* We try to leave things as clean as possible */
	rv = SCardReleaseContext(hContext);
	if (rv != SCARD_S_SUCCESS)
		printf("SCardReleaseContext: %s (0x%lX)\n", pcsc_stringify_error(rv),
			rv);

	/* Free allocated resources */
	SCardUnload();

	return 0;
} /* main */

