/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

/**
 * @file
 * @brief This abstracts dynamic library loading functions and timing.
 */

#include "config.h"

#include "pcsclite.h"
#include "debuglog.h"
#include "dyn_generic.h"

#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>

/*
 * / Load a module (if needed) 
 */
int DYN_LoadLibrary(void **pvLHandle, char *pcLibrary)
{

	CFStringRef bundlePath;
	CFURLRef bundleURL;
	CFBundleRef bundle;

	*pvLHandle = 0;

	/*
	 * @@@ kCFStringEncodingMacRoman might be wrong on non US systems. 
	 */

	bundlePath = CFStringCreateWithCString(NULL, pcLibrary,
		kCFStringEncodingMacRoman);
	if (bundlePath == NULL)
		return SCARD_E_NO_MEMORY;

	bundleURL = CFURLCreateWithFileSystemPath(NULL, bundlePath,
		kCFURLPOSIXPathStyle, TRUE);
	CFRelease(bundlePath);
	if (bundleURL == NULL)
		return SCARD_E_NO_MEMORY;

	bundle = CFBundleCreate(NULL, bundleURL);
	CFRelease(bundleURL);
	if (bundle == NULL)
		return SCARD_F_UNKNOWN_ERROR;

	if (!CFBundleLoadExecutable(bundle))
	{
		CFRelease(bundle);
		return SCARD_F_UNKNOWN_ERROR;
	}

	*pvLHandle = (void *) bundle;

	return SCARD_S_SUCCESS;
}

int DYN_CloseLibrary(void **pvLHandle)
{

	CFBundleRef bundle = (CFBundleRef) * pvLHandle;

	if (CFBundleIsExecutableLoaded(bundle) == TRUE)
	{
		CFBundleUnloadExecutable(bundle);
		CFRelease(bundle);
	}
	else
		Log1(PCSC_LOG_ERROR, "Cannot unload library.");

	*pvLHandle = 0;
	return SCARD_S_SUCCESS;
}

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, char *pcFunction)
{

	CFBundleRef bundle = (CFBundleRef) pvLHandle;
	CFStringRef cfName = CFStringCreateWithCString(NULL, pcFunction,
		kCFStringEncodingMacRoman);
	if (cfName == NULL)
		return SCARD_E_NO_MEMORY;

	*pvFHandle = CFBundleGetFunctionPointerForName(bundle, cfName);
	CFRelease(cfName);
	if (*pvFHandle == NULL)
		return SCARD_F_UNKNOWN_ERROR;

	return SCARD_S_SUCCESS;
}

#endif	/* __APPLE__ */
