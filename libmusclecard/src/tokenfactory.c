/*
 * This handles card abstraction attachment.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "PCSC/debuglog.h"
#include "PCSC/parser.h"
#include "dyn_generic.h"
#include "tokenfactory.h"

#ifndef WIN32
#ifndef MSC_SVC_DROPDIR
#define MSC_SVC_DROPDIR                     "/usr/local/pcsc/services"
#endif
#else
#define MSC_SVC_DROPDIR                     "C:\\Program Files\\Muscle\\Services"
#endif

#define MSC_MANUMSC_KEY_NAME                "spVendorName"
#define MSC_PRODMSC_KEY_NAME                "spProductName"
#define MSC_ATRMSC_KEY_NAME                 "spAtrValue"
#define MSC_LIBRMSC_KEY_NAME                "CFBundleExecutable"
#define MSC_DEFAULTAPP_NAME                 "spDefaultApplication"

int atrToString(MSCPUChar8 Atr, MSCULong32 Length, char *outAtr)
{
	MSCULong32 i;
	MSCULong32 j;

	j = 0;

	for (i = 0; i < Length; i++)
	{
		if ((Atr[i] / 16) > 9)
		{
			outAtr[j] = ((Atr[i] / 16) - 10) + 'A';
		} else
		{
			outAtr[j] = (Atr[i] / 16) + '0';
		}

		j += 1;

		if ((Atr[i] % 16) > 9)
		{
			outAtr[j] = ((Atr[i] % 16) - 10) + 'A';
		} else
		{
			outAtr[j] = (Atr[i] % 16) + '0';
		}

		j += 1;

	}

	outAtr[j] = 0;	/* Add the NULL */

	return 0;
}

int stringToBytes(char *inStr, MSCPUChar8 Buffer, MSCPULong32 Length)
{
	int i;
	int j;
	int inLen;

	j = 0;
	inLen = 0;

	inLen = strlen(inStr);

	if (inLen > MSC_MAXSIZE_AID)
	{
		return -1;
	}

	for (i = 0; i < inLen; i += 2)
	{
		if (inStr[i] <= '9' && inStr[i] >= '0')
		{
			Buffer[j] = (inStr[i] - '0') * 16;
		} else if (inStr[i] <= 'F' && inStr[i] >= 'A')
		{
			Buffer[j] = (inStr[i] - 'A' + 10) * 16;
		}

		if (inStr[i + 1] <= '9' && inStr[i + 1] >= '0')
		{
			Buffer[j] += inStr[i + 1] - '0';
		} else if (inStr[i + 1] <= 'F' && inStr[i + 1] >= 'A')
		{
			Buffer[j] += inStr[i + 1] - 'A' + 10;
		}

		j += 1;
	}

	*Length = j;

	return 0;
}

MSCLong32 TPSearchBundlesForAtr(MSCPUChar8 Atr, MSCULong32 Length,
	MSCLPTokenInfo tokenInfo)
{
	MSCLong32 rv;

#ifndef WIN32
	DIR *hpDir = 0;
	struct dirent *currFP = 0;
#else
	HANDLE hFind;
	WIN32_FIND_DATA findData;
	char findPath[200];
#endif

	char atrString[MAX_ATR_SIZE*2 +1]; /* ATR in ASCII */
	char fullPath[200];
	char fullLibPath[250];
	char keyValue[TOKEN_MAX_VALUE_SIZE];
	int atrIndex;

	rv = 0;
	atrIndex = 0;

	atrToString(Atr, Length, atrString);

#ifndef WIN32
	hpDir = opendir(MSC_SVC_DROPDIR);

	if (hpDir == 0)
	{
		DebugLogB("Cannot open PC/SC token drivers directory: %s",
			MSC_SVC_DROPDIR);
		return -1;
	}
#else
	sprintf(findPath, "%s\\*.bundle", MSC_SVC_DROPDIR);
	hFind = FindFirstFile(findPath, &findData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		DebugLogB("Cannot open PC/SC token drivers directory: %s", findPath);
		return -1;
	}
#endif

#ifndef WIN32
	while ((currFP = readdir(hpDir)) != 0)
	{
		if (strstr(currFP->d_name, ".bundle") != 0)
#else
	do
	{
		if (strstr(findData.cFileName, ".bundle") != 0)
#endif
		{

			/*
			 * The bundle exists - let's form a full path name and get the
			 * vendor and product ID's for this particular bundle 
			 */
#ifndef WIN32
			sprintf(fullPath, "%s/%s/Contents/Info.plist", MSC_SVC_DROPDIR,
				currFP->d_name);
#else
			sprintf(fullPath, "%s\\%s\\Contents\\Info.plist", MSC_SVC_DROPDIR,
				findData.cFileName);
#endif

			atrIndex = 0;

#ifndef NO_MSC_DEBUG
			DebugLogB("ATR comparison: FILE: %s", fullPath);
			DebugLogB("ATR comparison: Target Match: %s", atrString);
#endif

			while (1)
			{
				rv = LTPBundleFindValueWithKey(fullPath,
					MSC_ATRMSC_KEY_NAME, keyValue, atrIndex);
				if (rv != 0)
				{
					break;	/* No aliases found, break out of search
							 * aliases loop */
				}
#ifndef NO_MSC_DEBUG
				DebugLogB("ATR comparison: Source: %s", keyValue);
#endif

				if (strcmp(keyValue, atrString) != 0)
				{
					/*
					 * Go back and see if there are any aliases 
					 */
					atrIndex += 1;
					continue;
				}
#ifndef NO_MSC_DEBUG
				DebugLogB("Match found at ATR alias %d", atrIndex);
#endif

				/*
				 * See if this bundle has a special name for this ATR 
				 */
				rv = LTPBundleFindValueWithKey(fullPath,
					MSC_PRODMSC_KEY_NAME, keyValue, atrIndex);
				if (rv != 0)
				{
					rv = LTPBundleFindValueWithKey(fullPath,
						MSC_PRODMSC_KEY_NAME, keyValue, 0);
					if (rv != 0)
					{
						DebugLogA
							("Match found, failed due to no product name.");
#ifndef WIN32
						closedir(hpDir);
#endif
						return -1;
					}
				}
#ifndef NO_MSC_DEBUG
				DebugLogB("Product name: %s", keyValue);
#endif
				strcpy(tokenInfo->tokenName, keyValue);

				/*
				 * See if this bundle has a special driver for this card 
				 */
				rv = LTPBundleFindValueWithKey(fullPath,
					MSC_LIBRMSC_KEY_NAME, keyValue, atrIndex);
				if (rv != 0)
				{
					rv = LTPBundleFindValueWithKey(fullPath,
						MSC_LIBRMSC_KEY_NAME, keyValue, 0);
					if (rv != 0)
					{
						DebugLogA
							("Match found, failed due to no library path.");
#ifndef WIN32
						closedir(hpDir);
#endif
						return -1;
					}
				}
#if defined(WIN32)
				sprintf(fullLibPath, "%s\\%s\Contents\\Win32\\%s",
					MSC_SVC_DROPDIR, findData.cFileName, keyValue);
#elif defined(__APPLE__)
				sprintf(fullLibPath, "%s/%s", MSC_SVC_DROPDIR,
					currFP->d_name);
#else
				sprintf(fullLibPath, "%s/%s/Contents/%s/%s", MSC_SVC_DROPDIR,
					currFP->d_name, PCSC_ARCH, keyValue);
#endif

				if (fullLibPath == NULL)
				{
					DebugLogA("No path to bundle library found !");
					return -1;
				}

				/*
				 * Copy the library path and return successfully 
				 */
				strcpy(tokenInfo->svProvider, fullLibPath);

				/*
				 * See if this bundle has a default AID 
				 */
				rv = LTPBundleFindValueWithKey(fullPath,
					MSC_DEFAULTAPP_NAME, keyValue, atrIndex);
				if (rv != 0)
				{
					rv = LTPBundleFindValueWithKey(fullPath,
						MSC_DEFAULTAPP_NAME, keyValue, 0);
				}

				if (rv == 0)
				{
#ifndef NO_MSC_DEBUG
					DebugLogB("Default AID name: %s", keyValue);
#endif
					rv = stringToBytes(keyValue, tokenInfo->tokenApp,
						&tokenInfo->tokenAppLen);
					if (rv != 0)
					{
						DebugLogA
							("Match found, failed due to malformed aid string.");
#ifndef WIN32
						closedir(hpDir);
#endif
						return -1;
					}

				} else
				{
					DebugLogA("No AID specified in bundle");
					tokenInfo->tokenAppLen = 0;
				}

#ifndef WIN32
				closedir(hpDir);
#endif
				return 0;

			}	/* do ... while */
		}	/* if .bundle */
	}	/* while readdir */
#ifdef WIN32
	/* This is part of a Do..While loop (see above) */
	while (FindNextFile(hFind, &findData) != 0);
#endif

#ifndef WIN32
	closedir(hpDir);
#endif
	return -1;
}

MSCLong32 TPLoadToken(MSCLPTokenConnection pConnection)
{
	MSCLong32 rv;

	pConnection->libPointers.pvfWriteFramework = 0;
	pConnection->libPointers.pvfInitializePlugin = 0;
	pConnection->libPointers.pvfFinalizePlugin = 0;
	pConnection->libPointers.pvfGetStatus = 0;
	pConnection->libPointers.pvfGetCapabilities = 0;
	pConnection->libPointers.pvfExtendedFeature = 0;
	pConnection->libPointers.pvfGenerateKeys = 0;
	pConnection->libPointers.pvfImportKey = 0;
	pConnection->libPointers.pvfExportKey = 0;
	pConnection->libPointers.pvfComputeCrypt = 0;
	pConnection->libPointers.pvfExtAuthenticate = 0;
	pConnection->libPointers.pvfListKeys = 0;
	pConnection->libPointers.pvfCreatePIN = 0;
	pConnection->libPointers.pvfVerifyPIN = 0;
	pConnection->libPointers.pvfChangePIN = 0;
	pConnection->libPointers.pvfUnblockPIN = 0;
	pConnection->libPointers.pvfListPINs = 0;
	pConnection->libPointers.pvfCreateObject = 0;
	pConnection->libPointers.pvfDeleteObject = 0;
	pConnection->libPointers.pvfWriteObject = 0;
	pConnection->libPointers.pvfReadObject = 0;
	pConnection->libPointers.pvfListObjects = 0;
	pConnection->libPointers.pvfLogoutAll = 0;
	pConnection->libPointers.pvfGetChallenge = 0;

	/*
	 * Find the Card's Library 
	 */

	rv = TPSearchBundlesForAtr(pConnection->tokenInfo.tokenId,
		pConnection->tokenInfo.tokenIdLength, &pConnection->tokenInfo);

	if (rv != 0)
	{
		DebugLogA("Error: Matching Token ATR Not Found.");
		DebugXxd("ATR: ", pConnection->tokenInfo.tokenId,
			pConnection->tokenInfo.tokenIdLength);

		return SCARD_E_CARD_UNSUPPORTED;
	}

	/*
	 * Load that library and store the handle in the SCARDCHANNEL
	 * structure 
	 */

	rv = DYN_LoadLibrary(&pConnection->tokenLibHandle,
		pConnection->tokenInfo.svProvider);

	if (rv != SCARD_S_SUCCESS)
	{
		DebugLogA("Error: Could not load service library");
		DebugLogB("->> %s", pConnection->tokenInfo.svProvider);
		return SCARD_E_INVALID_TARGET;
	} else
	{
		DebugLogB("Loading service library %s",
			pConnection->tokenInfo.svProvider);
	}

	rv = TPBindFunctions(pConnection);

	return rv;
}

MSCLong32 TPUnloadToken(MSCLPTokenConnection pConnection)
{
	MSCLong32 rv;

	if (pConnection->tokenLibHandle == 0)
	{
		return SCARD_E_INVALID_VALUE;
	}

	rv = DYN_CloseLibrary(&pConnection->tokenLibHandle);

	if (rv != SCARD_S_SUCCESS)
	{
		return rv;
	}

	pConnection->tokenLibHandle = 0;
	return TPUnbindFunctions(pConnection);
}

MSCLong32 TPBindFunctions(MSCLPTokenConnection pConnection)
{
	MSCLong32 rv;

	if (pConnection->tokenLibHandle == 0)
	{
		return SCARD_E_INVALID_TARGET;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfWriteFramework,
		"PL_MSCWriteFramework");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfWriteFramework = 0;
		DebugLogA("Missing functions");
		/*
		 * No big deal - this feature is just not supported 
		 */
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfIdentifyToken, "PL_MSCIdentifyToken");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfIdentifyToken = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfInitializePlugin,
		"PL_MSCInitializePlugin");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfInitializePlugin = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfFinalizePlugin,
		"PL_MSCFinalizePlugin");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfFinalizePlugin = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfGetStatus, "PL_MSCGetStatus");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfGetStatus = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfGetCapabilities,
		"PL_MSCGetCapabilities");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfGetCapabilities = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfExtendedFeature,
		"PL_MSCExtendedFeature");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfExtendedFeature = 0;
		DebugLogA("Missing functions");
		/*
		 * No big deal - there are no extended features 
		 */
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfGenerateKeys, "PL_MSCGenerateKeys");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfGenerateKeys = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfImportKey, "PL_MSCImportKey");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfImportKey = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfExportKey, "PL_MSCExportKey");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfExportKey = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfComputeCrypt, "PL_MSCComputeCrypt");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfComputeCrypt = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfExtAuthenticate,
		"PL_MSCExtAuthenticate");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfExtAuthenticate = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfListKeys, "PL_MSCListKeys");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfListKeys = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfCreatePIN, "PL_MSCCreatePIN");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfCreatePIN = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfVerifyPIN, "PL_MSCVerifyPIN");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfVerifyPIN = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfChangePIN, "PL_MSCChangePIN");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfChangePIN = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfUnblockPIN, "PL_MSCUnblockPIN");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfUnblockPIN = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfListPINs, "PL_MSCListPINs");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfListPINs = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfCreateObject, "PL_MSCCreateObject");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfCreateObject = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfDeleteObject, "PL_MSCDeleteObject");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfDeleteObject = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfWriteObject, "PL_MSCWriteObject");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfWriteObject = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfReadObject, "PL_MSCReadObject");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfReadObject = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfListObjects, "PL_MSCListObjects");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfListObjects = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfLogoutAll, "PL_MSCLogoutAll");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfLogoutAll = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	rv = DYN_GetAddress(pConnection->tokenLibHandle,
		&pConnection->libPointers.pvfGetChallenge, "PL_MSCGetChallenge");

	if (rv != SCARD_S_SUCCESS)
	{
		pConnection->libPointers.pvfGetChallenge = 0;
		DebugLogA("Missing functions");
		return SCARD_F_INTERNAL_ERROR;
	}

	return SCARD_S_SUCCESS;
}

MSCLong32 TPUnbindFunctions(MSCLPTokenConnection pConnection)
{
	pConnection->libPointers.pvfWriteFramework = 0;
	pConnection->libPointers.pvfInitializePlugin = 0;
	pConnection->libPointers.pvfFinalizePlugin = 0;
	pConnection->libPointers.pvfGetStatus = 0;
	pConnection->libPointers.pvfGetCapabilities = 0;
	pConnection->libPointers.pvfExtendedFeature = 0;
	pConnection->libPointers.pvfGenerateKeys = 0;
	pConnection->libPointers.pvfImportKey = 0;
	pConnection->libPointers.pvfExportKey = 0;
	pConnection->libPointers.pvfComputeCrypt = 0;
	pConnection->libPointers.pvfExtAuthenticate = 0;
	pConnection->libPointers.pvfListKeys = 0;
	pConnection->libPointers.pvfCreatePIN = 0;
	pConnection->libPointers.pvfVerifyPIN = 0;
	pConnection->libPointers.pvfChangePIN = 0;
	pConnection->libPointers.pvfUnblockPIN = 0;
	pConnection->libPointers.pvfListPINs = 0;
	pConnection->libPointers.pvfCreateObject = 0;
	pConnection->libPointers.pvfDeleteObject = 0;
	pConnection->libPointers.pvfWriteObject = 0;
	pConnection->libPointers.pvfReadObject = 0;
	pConnection->libPointers.pvfListObjects = 0;
	pConnection->libPointers.pvfLogoutAll = 0;
	pConnection->libPointers.pvfGetChallenge = 0;

	return SCARD_S_SUCCESS;
}
