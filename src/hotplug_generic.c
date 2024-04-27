/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2000-2003
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2022
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
 * @brief This provides a search API for hot pluggble devices.
 *
 * Check for platforms that have their own specific support.
 * It's easier and flexible to do it here, rather than
 * with automake conditionals in src/Makefile.am.
 * No, it's still not a perfect solution design wise.
 */

#include "config.h"
#include "hotplug.h"

#if !defined(__APPLE__) && !defined(HAVE_LIBUSB) && !defined(HAVE_LIBUDEV)

LONG HPSearchHotPluggables(const char * hpDirPath)
{
	(void)hpDirPath;

	return 0;
}

ULONG HPRegisterForHotplugEvents(const char * hpDirPath)
{
	(void)hpDirPath;

	return 0;
}

LONG HPStopHotPluggables(void)
{
	return 0;
}

void HPReCheckSerialReaders(void)
{
}

#endif
