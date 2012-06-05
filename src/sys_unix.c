/*
 * This handles abstract system level calls.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles abstract system level calls.
 */

#include "config.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

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

INTERNAL int SYS_RandomInt(int fStart, int fEnd)
{
	static int iInitialized = 0;
	int iRandNum = 0;

	if (0 == iInitialized)
	{
		srand(SYS_GetSeed());
		iInitialized = 1;
	}

	if (-1 == fEnd)
		/* full int range */
		iRandNum = rand();
	else
		iRandNum = ((rand()+0.0)/RAND_MAX * (fEnd - fStart)) + fStart;

	return iRandNum;
}

INTERNAL int SYS_GetSeed(void)
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
	return myseed;
}

