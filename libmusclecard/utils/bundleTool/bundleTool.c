/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : bundleTool.c
            Package: MuscleCard Framework
            Author : David Corcoran
            Date   : 03/11/01
            License: Copyright (C) 2002 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This automatically updates the Info.plist
 
            You may not remove this header from this file
            without prior permission from the author.
 
********************************************************************/

#ifdef MSC_TARGET_OSX
#include <PCSC/wintypes.h>
#include <PCSC/winscard.h>
#else
#include <wintypes.h>
#include <winscard.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * The following defines personalize this for different tokens 
 */

#define BUNDLE_DIR            "/usr/local/pcsc/services/"

/*
 * End of personalization 
 */

#define CHECK_ERR(cond, msg) { if (cond) { \
  printf("Error: %s\n", msg); return -1; } }

int main(int argc, char **argv)
{

	LONG rv;
	SCARDCONTEXT hContext;
	SCARD_READERSTATE_A rgReaderStates;
	DWORD readerListSize;
	struct stat statBuffer;
	char spAtrValue[100];
	char chosenInfoPlist[1024];
	char *readerList;
	char *restFile;
	char atrInsertion[256];
	FILE *fp;
	DIR *bundleDir;
	struct dirent *currBundle;
	int i, p;
	int userChoice;
	int totalBundles;
	int filePosition;
	int restFileSize;
	int restOffset;
	int getsSize;

	if (argc > 1)
	{
		printf("Invalid arguments\n");
		printf("./bundleTool\n");
		return -1;
	}

	currBundle = 0;

	bundleDir = opendir(BUNDLE_DIR);
	CHECK_ERR(bundleDir == 0, "Opendir failed");

	printf("Select the approprate token driver:\n");
	printf("-----------------------------------\n");

	i = 1;
	totalBundles = 0;

	while ((currBundle = readdir(bundleDir)) != 0)
	{
		if (strstr(currBundle->d_name, ".bundle") != 0)
		{
			printf("  %d.     %s\n", i++, currBundle->d_name);
			totalBundles += 1;
		}
	}
	printf("-----------------------------------\n");

	if (totalBundles == 0)
	{
		printf("No drivers exist - exiting\n");
		return 1;
	}

	do
	{
		printf("Enter the number: ");
		scanf("%d", &userChoice);
	}
	while (userChoice < 1 && userChoice > totalBundles);

	closedir(bundleDir);

	bundleDir = opendir(BUNDLE_DIR);
	CHECK_ERR(bundleDir == 0, "Opendir failed");

	do
	{
		if ((currBundle = readdir(bundleDir)) != 0)
		{
			if (strstr(currBundle->d_name, ".bundle") != 0)
			{
				userChoice -= 1;
			}
		}
	}
	while (userChoice != 0);

	snprintf(chosenInfoPlist, sizeof(chosenInfoPlist),
		"%s%s/Contents/Info.plist", BUNDLE_DIR, currBundle->d_name);
	closedir(bundleDir);
	printf("\n");

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, 0, 0, &hContext);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "EstablishContext Failed");

	readerListSize = 0;
	rv = SCardListReaders(hContext, 0, 0, &readerListSize);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "ListReaders Failed");

	readerList = (char *) malloc(sizeof(char) * readerListSize);
	CHECK_ERR(readerList == 0, "Malloc Failed");

	rv = SCardListReaders(hContext, 0, readerList, &readerListSize);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "ListReaders Alloc Failed");

	printf("Insert your token in: %s\n", readerList);

	rgReaderStates.szReader = readerList;
	rgReaderStates.dwCurrentState = SCARD_STATE_EMPTY;

	rv = SCardGetStatusChange(hContext, INFINITE, &rgReaderStates, 1);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "GetStatusChange Failed");

	p = 0;
	for (i = 0; i < rgReaderStates.cbAtr; i++)
	{
		sprintf(&spAtrValue[p], "%02X", rgReaderStates.rgbAtr[i]);
		p += 2;
	}
	printf("\n");

	snprintf(atrInsertion, sizeof(atrInsertion),
		"        <string>%s</string>\n", spAtrValue);

	fp = fopen(chosenInfoPlist, "r+");
	if (fp == 0)
	{
		printf("Couldn't open %s\n", chosenInfoPlist);
	}
	CHECK_ERR(fp == 0, "Opening of Info.plist failed.");

	rv = stat(chosenInfoPlist, &statBuffer);
	CHECK_ERR(rv != 0, "Stat failed\n");

	restFileSize = statBuffer.st_size + strlen(atrInsertion);
	restFile = (char *) malloc(sizeof(char) * restFileSize);
	CHECK_ERR(restFile == 0, "Malloc failed");

	filePosition = 0;
	restOffset = 0;
	getsSize = 0;

	do
	{
		if (fgets(&restFile[restOffset], restFileSize, fp) == 0)
		{
			break;
		}

		if (strstr(&restFile[restOffset], "<key>spAtrValue</key>"))
		{
			filePosition = ftell(fp);
		}

		getsSize = strlen(&restFile[restOffset]);
		restOffset += getsSize;
	}
	while (1);

	rewind(fp);
	fwrite(restFile, 1, filePosition, fp);
	fwrite(atrInsertion, 1, strlen(atrInsertion), fp);
	fwrite(&restFile[filePosition], 1,
		statBuffer.st_size - filePosition, fp);

	fclose(fp);

	printf("Token support updated successfully !\n");

	return 0;
}

