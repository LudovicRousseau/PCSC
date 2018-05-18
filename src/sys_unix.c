/*
 * This handles abstract system level calls.
 *
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999
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
 * @brief This handles abstract system level calls.
 */

#include "config.h"
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>

#include "misc.h"
#include "sys_generic.h"

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
 * @param[in] fStart minimal value
 * @param[in] fEnd maximal value or -1 for a full range
 *
 * @return a random number between fStart and fEnd
 */
INTERNAL int SYS_RandomInt(int fStart, int fEnd)
{
	int iRandNum = 0;

	if (-1 == fEnd)
		/* full int range */
		iRandNum = rand();
	else
		iRandNum = ((rand()+0.0)/RAND_MAX * (fEnd - fStart)) + fStart;

	return iRandNum;
}

/**
 * Initialize the random generator
 */
INTERNAL void SYS_InitRandom(void)
{
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

	srand(myseed);
}

