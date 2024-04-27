/*
 * This handles abstract system level calls.
 *
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2024
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
 * @brief This handles abstract system level calls.
 */

#include "config.h"
#define _GNU_SOURCE /* for secure_getenv(3) */
#include <sys/time.h>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#endif /* HAVE_GETRANDOM */
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "misc.h"
#include "sys_generic.h"
#include "debuglog.h"

/**
 * @brief Makes the current process sleep for some seconds.
 *
 * @param[in] iTimeVal Number of seconds to sleep.
 */
INTERNAL int SYS_Sleep(int iTimeVal)
{
#ifdef HAVE_NANOSLEEP
	struct timespec mrqtp;
	mrqtp.tv_sec = iTimeVal;
	mrqtp.tv_nsec = 0;

	return nanosleep(&mrqtp, NULL);
#else
	return sleep(iTimeVal);
#endif
}

/**
 * @brief Makes the current process sleep for some microseconds.
 *
 * @param[in] iTimeVal Number of microseconds to sleep.
 */
INTERNAL int SYS_USleep(int iTimeVal)
{
#ifdef HAVE_NANOSLEEP
	struct timespec mrqtp;
	mrqtp.tv_sec = iTimeVal/1000000;
	mrqtp.tv_nsec = (iTimeVal - (mrqtp.tv_sec * 1000000)) * 1000;

	return nanosleep(&mrqtp, NULL);
#else
	struct timeval tv;
	tv.tv_sec  = iTimeVal/1000000;
	tv.tv_usec = iTimeVal - (tv.tv_sec * 1000000);
	return select(0, NULL, NULL, NULL, &tv);
#endif
}

/**
 * Generate a pseudo random number
 *
 * @return a non-negative random number
 *
 * @remark the range is at least up to `2^31`.
 * @remark this is a CSPRNG when `getrandom()` is available, LCG otherwise.
 * @warning SYS_InitRandom() should be called (once) before using this function.
 * @warning not thread safe when system lacks `getrandom()` syscall.
 * @warning not cryptographically secure when system lacks `getrandom()` syscall.
 * @warning if interrupted by a signal, this function may return 0.
 */
INTERNAL int SYS_RandomInt(void)
{
#ifdef HAVE_GETRANDOM
	unsigned int ui = 0;
	unsigned char c[sizeof ui] = {0};
	size_t i;
	ssize_t ret;

	ret = getrandom(c, sizeof c, 0);
	if (-1 == ret)
	{
#ifdef PCSCD
		Log2(PCSC_LOG_ERROR, "getrandom() failed: %s", strerror(errno));
#endif
		return lrand48();
	}
	// this loop avoids trap representations that may occur in the naive solution
	for(i = 0; i < sizeof ui; i++) {
		ui <<= CHAR_BIT;
		ui |= c[i];
	}
	// the casts are for the sake of clarity
	return (int)(ui & (unsigned int)INT_MAX);
#else
	int r = lrand48(); // this is not thread-safe
	return r;
#endif /* HAVE_GETRANDOM */
}

/**
 * Initialize the random generator
 */
INTERNAL void SYS_InitRandom(void)
{
#ifndef HAVE_GETRANDOM
	struct timeval tv;
	struct timezone tz;
	long myseed = 0;

	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	if (gettimeofday(&tv, &tz) == 0)
	{
		myseed = tv.tv_usec;
	} else
	{
		myseed = (long) time(NULL);
	}

	srand48(myseed);
#endif /* HAVE_GETRANDOM */
}

/**
 * (More) secure version of getenv(3)
 *
 * @param[in] name variable environment name
 *
 * @return value of the environment variable called "name"
 */
INTERNAL const char * SYS_GetEnv(const char *name)
{
#ifdef HAVE_SECURE_GETENV
	return secure_getenv(name);
#else
	/* Otherwise, make sure current process is not tainted by uid or gid
	 * changes */
#ifdef HAVE_issetugid
	if (issetugid())
		return NULL;
#endif
	return getenv(name);
#endif
}

