/*
 * This handles abstract system level calls.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
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

#ifndef HAVE_DAEMON
static INTERNAL int SYS_Fork(void)
{
	return fork();
}
#endif

/**
 * @brief put the process to run in the background.
 *
 * @param[in] nochdir if zero, change the current directory to "/".
 * @param[in] noclose if zero, redirect standard imput/output/error to /dev/nulll.
 *
 * @return error code.
 * @retval 0 success.
 * @retval -1 an error ocurred.
 */
INTERNAL int SYS_Daemon(int nochdir, int noclose)
{
#ifdef HAVE_DAEMON
	return daemon(nochdir, noclose);
#else

#if defined(__SVR4) && defined(__sun)
	pid_t pid;

	pid = SYS_Fork();
	if (-1 == pid)
	{
		Log2(PCSC_LOG_CRITICAL, "main: SYS_Fork() failed: %s", strerror(errno));
		return -1;
	}
	else
	{
		if (pid != 0)
			/* the father exits */
			exit(0);
	}

	setsid();

	pid = SYS_Fork();
	if (-1 == pid)
	{
		Log2(PCSC_LOG_CRITICAL, "main: SYS_Fork() failed: %s", strerror(errno));
		exit(1);
	}
	else
	{
		if (pid != 0)
			/* the father exits */
			exit(0);
	}
#else
	switch (SYS_Fork())
	{
	case -1:
		return (-1);
	case 0:
		break;
	default:
		return (0);
	}
#endif

	if (!noclose) {
		if (SYS_CloseFile(0))
			Log2(PCSC_LOG_ERROR, "SYS_CloseFile(0) failed: %s",
				strerror(errno));

		if (SYS_CloseFile(1))
			Log2(PCSC_LOG_ERROR, "SYS_CloseFile(1) failed: %s",
				strerror(errno));

		if (SYS_CloseFile(2))
			Log2(PCSC_LOG_ERROR, "SYS_CloseFile(2) failed: %s",
				strerror(errno));
	}
	if (!nochdir) {
		if (SYS_Chdir("/"))
			Log2(PCSC_LOG_ERROR, "SYS_Chdir() failed: %s", strerror(errno));
	}
	return 0;
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

