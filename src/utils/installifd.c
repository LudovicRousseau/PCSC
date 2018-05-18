/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2000-2003
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
 * @brief This is a reader installer for pcsc-lite.
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include "PCSC/wintypes.h"
#include "PCSC/winscard.h"

#ifndef PCSCLITE_READER_CONFIG
#define PCSCLITE_READER_CONFIG "/etc/reader.conf"
#endif

int main(/*@unused@*/ int argc, /*@unused@*/ char *argv[])
{

	struct stat statbuf;
	char lpcReader[MAX_READERNAME];
	char lpcLibrary[FILENAME_MAX];
	char *lpcPortID = NULL;
	int iPort;
	int iStat;
	FILE *fd;

	printf("Please enter a friendly name for your reader (%d char max)\n",
		(int)sizeof(lpcReader));
	printf("-----> ");

	(void)fgets(lpcReader, sizeof(lpcReader), stdin);

	/* remove trailing \n */
	lpcReader[strlen(lpcReader)-1] = '\0';

  retrylib:

	printf("Please enter the full path of the readers driver (%d char max)\n",
		(int)sizeof(lpcLibrary));
	printf("Example: %s/librdr_generic.so\n", PCSCLITE_HP_DROPDIR);
	printf("-----> ");

	(void)fgets(lpcLibrary, sizeof(lpcLibrary), stdin);

	/* remove trailing \n */
	lpcLibrary[strlen(lpcLibrary)-1] = '\0';

	iStat = stat(lpcLibrary, &statbuf);

	if (iStat != 0)
	{
		/*
		 * Library does not exist
		 */
		printf("Library path %s does not exist.\n\n", lpcLibrary);
		goto retrylib;
	}

	printf("Please select the I/O port from the list below:\n");
	printf("------------------------------------------------\n");
	printf("1) COM1 (Serial Port 1)\n");
	printf("2) COM2 (Serial Port 2)\n");
	printf("3) COM3 (Serial Port 3)\n");
	printf("4) COM4 (Serial Port 4)\n");
	printf("5) LPT1 (Parallel Port 1)\n");
	printf("6) USR1 (Sym Link Defined)\n");
	printf("------------------------------------------------\n");

  retryport:

	printf("\n");
	printf("Enter number (1-6): ");

	if ((scanf("%d", &iPort) != 1) || iPort < 1 || iPort > 6)
	{
		printf("Invalid input (%d) please choose (1-5)\n", iPort);
		/* eat the \n */
		(void)getchar();
		goto retryport;
	}

	switch (iPort)
	{
		case 1:
			lpcPortID = "0x0103F8";
			break;
		case 2:
			lpcPortID = "0x0102F8";
			break;
		case 3:
			lpcPortID = "0x0103E8";
			break;
		case 4:
			lpcPortID = "0x0102E8";
			break;
		case 5:
			lpcPortID = "0x020378";
			break;
		case 6:
			lpcPortID = "0x000001";
			break;
	}

	printf("\n\n");
	printf("Now creating new " PCSCLITE_READER_CONFIG "\n");

	fd = fopen(PCSCLITE_READER_CONFIG, "w");

	if (fd == NULL)
	{
		printf("Cannot open file %s: %s\n", PCSCLITE_READER_CONFIG, strerror(errno));
		return 1;
	}

	fprintf(fd, "%s", "# Configuration file for pcsc-lite\n");
	fprintf(fd, "%s", "# David Corcoran <corcoran@musclecard.com\n");

	fprintf(fd, "%s", "\n\n");

	fprintf(fd, "FRIENDLYNAME     \"%s\"\n", lpcReader);
	fprintf(fd, "DEVICENAME       /dev/null\n");
	fprintf(fd, "LIBPATH          %s\n", lpcLibrary);
	fprintf(fd, "CHANNELID        %s\n", lpcPortID);

	fprintf(fd, "%s", "\n\n");

	fprintf(fd, "%s", "# End of file\n");
	return 0;
}

