/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2010
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
 * @brief This abstracts dynamic library loading functions and timing.
 */

#include "config.h"

#include "misc.h"
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
	{
		Log1(PCSC_LOG_ERROR, "CFBundleCreate");
		return SCARD_F_UNKNOWN_ERROR;
	}

	if (!CFBundleLoadExecutable(bundle))
	{
		Log1(PCSC_LOG_ERROR, "CFBundleLoadExecutable");
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

int DYN_GetAddress(void *pvLHandle, void **pvFHandle, const char *pcFunction,
	int mayfail)
{
	(void)mayfail;

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
