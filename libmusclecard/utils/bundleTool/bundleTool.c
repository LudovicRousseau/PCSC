/*
 * This automatically updates the Info.plist.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2001-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

#include "pcsclite.h"
#include "winscard.h"

/*
 * The following defines personalize this for different tokens 
 */

#define BUNDLE_DIR MSC_SVC_DROPDIR

/* redefine MSC_SVC_DROPDIR only if not yet defined */
#ifndef MSC_SVC_DROPDIR
#ifndef WIN32
#define MSC_SVC_DROPDIR      "/usr/local/musclecard/services"
#else
#define MSC_SVC_DROPDIR      "C:\\Program Files\\Muscle\\Services"
#endif
#endif


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
#ifndef WIN32
	DIR *bundleDir;
	struct dirent *currBundle;
#else
	HANDLE hFind;
	WIN32_FIND_DATA findData;
	char findPath[200];
	int find;
#endif
	int i, p;
	int userChoice;
	int totalBundles;
	int filePosition;
	int restFileSize;
	int restOffset;
	int getsSize;

	if (argc > 1)
	{
		printf("Invalid arguments\nUsage: %s\n", argv[0]);
		return -1;
	}

#ifndef WIN32
	currBundle = NULL;
	bundleDir = opendir(BUNDLE_DIR);
	if (bundleDir == NULL)
	{
		printf("Opendir failed %s: %s\n", BUNDLE_DIR, strerror(errno));
		return -1;
	}
#else
	sprintf(findPath, "%s\\*.bundle", BUNDLE_DIR);
	hFind = FindFirstFile(findPath, &findData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("Cannot open PC/SC token drivers directory: %s", findPath);
		return -1;
	}
#endif

	printf("Select the appropriate token driver:\n");
	printf("------------------------------------\n");

	i = 1;
	totalBundles = 0;
#ifndef WIN32
	while ((currBundle = readdir(bundleDir)) != NULL)
	{
		if (strstr(currBundle->d_name, ".bundle") != NULL)
		{
			printf("  %d.     %s\n", i++, currBundle->d_name);
			totalBundles += 1;
		}
	}
#else
	do
	{
		if (strstr(findData.cFileName, ".bundle") != NULL)
		{
			printf("  %d.     %s\n", i++, findData.cFileName);
			totalBundles += 1;
		}
	} while (FindNextFile(hFind, &findData) != 0);
#endif

	printf("------------------------------------\n");

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

#ifndef WIN32
	closedir(bundleDir);
#endif

#ifndef WIN32
	bundleDir = opendir(BUNDLE_DIR);
	CHECK_ERR(bundleDir == NULL, "Opendir failed");
#else
	sprintf(findPath, "%s\\*.bundle", BUNDLE_DIR);
	hFind = FindFirstFile(findPath, &findData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		printf("Cannot open PC/SC token drivers directory: %s", findPath);
		return -1;
	}
	else
		find = 1;
#endif

	do
	{
#ifndef WIN32
		if ((currBundle = readdir(bundleDir)) != NULL)
		{
			if (strstr(currBundle->d_name, ".bundle") != NULL)
				userChoice -= 1;
		}
#else
		if (find)
		{
			if (strstr(findData.cFileName, ".bundle") != NULL)
				userChoice -= 1;
		}

		if (userChoice)
			find = (FindNextFile(hFind, &findData) != 0);
#endif
	} while (userChoice != 0);


#ifndef WIN32
#if HAVE_SNPRINTF
	snprintf(chosenInfoPlist, sizeof(chosenInfoPlist),
		"%s/%s/Contents/Info.plist", BUNDLE_DIR, currBundle->d_name);
#else
	sprintf(chosenInfoPlist, "%s/%s/Contents/Info.plist", BUNDLE_DIR, 
		currBundle->d_name);
#endif
#else
	sprintf(chosenInfoPlist, "%s\\%s\\Contents\\Info.plist", BUNDLE_DIR,
		findData.cFileName);
#endif

#ifndef WIN32
	closedir(bundleDir);
#endif
	printf("\n");

	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "EstablishContext Failed");

	readerListSize = 0;
	rv = SCardListReaders(hContext, NULL, NULL, &readerListSize);
	CHECK_ERR(rv != SCARD_S_SUCCESS, "ListReaders Failed");

	readerList = (char *) malloc(sizeof(char) * readerListSize);
	CHECK_ERR(readerList == NULL, "Malloc Failed");

	rv = SCardListReaders(hContext, NULL, readerList, &readerListSize);
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

#if HAVE_SNPRINTF
	snprintf(atrInsertion, sizeof(atrInsertion),
		"        <string>%s</string>\n", spAtrValue);
#else 
	sprintf(atrInsertion, "        <string>%s</string>\n", spAtrValue);
#endif

	fp = fopen(chosenInfoPlist, "r+");
	if (fp == NULL)
	{
		printf("Couldn't open %s: %s\n", chosenInfoPlist, strerror(errno));
		return -1;
	}

	rv = stat(chosenInfoPlist, &statBuffer);
	CHECK_ERR(rv != 0, "Stat failed");

	restFileSize = statBuffer.st_size + strlen(atrInsertion);
	restFile = (char *) malloc(sizeof(char) * restFileSize);
	CHECK_ERR(restFile == NULL, "Malloc failed");

	filePosition = 0;
	restOffset = 0;
	getsSize = 0;

	do
	{
		if (fgets(&restFile[restOffset], restFileSize, fp) == NULL)
			break;

		if (strstr(&restFile[restOffset], "<key>spAtrValue</key>"))
			filePosition = ftell(fp);

		getsSize = strlen(&restFile[restOffset]);
		restOffset += getsSize;
	}
	while (1);

	rewind(fp);
	fwrite(restFile, 1, filePosition, fp);
	fwrite(atrInsertion, 1, strlen(atrInsertion), fp);
	fwrite(&restFile[filePosition], 1, statBuffer.st_size - filePosition, fp);

	fclose(fp);

	printf("Token support updated successfully !\n");

	return 0;
}

