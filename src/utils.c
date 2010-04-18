/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2006-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: pcscdaemon.c 2377 2007-02-05 13:13:56Z rousseau $
 */

/**
 * @file
 * @brief utility functions
 */

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>

#include "config.h"
#include "debuglog.h"
#include "utils.h"
#include "pcscd.h"
#include "sys_generic.h"

pid_t GetDaemonPid(void)
{
	FILE *f;
	pid_t pid;

	/* pids are only 15 bits but 4294967296
	 * (32 bits in case of a new system use it) is on 10 bytes
	 */
	if ((f = fopen(PCSCLITE_RUN_PID, "rb")) != NULL)
	{
		char pid_ascii[PID_ASCII_SIZE];

		(void)fgets(pid_ascii, PID_ASCII_SIZE, f);
		(void)fclose(f);

		pid = atoi(pid_ascii);
	}
	else
	{
		Log2(PCSC_LOG_CRITICAL, "Can't open " PCSCLITE_RUN_PID ": %s",
			strerror(errno));
		return -1;
	}

	return pid;
} /* GetDaemonPid */

int SendHotplugSignal(void)
{
	pid_t pid;

	pid = GetDaemonPid();

	if (pid != -1)
	{
		Log2(PCSC_LOG_INFO, "Send hotplug signal to pcscd (pid=%d)", pid);
		if (kill(pid, SIGUSR1) < 0)
		{
			Log3(PCSC_LOG_CRITICAL, "Can't signal pcscd (pid=%d): %s",
				pid, strerror(errno));
			return EXIT_FAILURE ;
		}
		(void)SYS_Sleep(1);
	}

	return EXIT_SUCCESS;
} /* SendHotplugSignal */

/**
 * Check is OpenCT is running and display a critical message if it is
 *
 * The first cause of pcsc-lite failure is that OpenCT is installed and running
 * and has already claimed the USB device. In that case RFAddReader() fails
 * and I get a user support request
 */
#define OPENCT_FILE "/var/run/openct/status"
int CheckForOpenCT(void)
{
	struct stat buf;

	if (0 == stat(OPENCT_FILE, &buf))
	{
		Log1(PCSC_LOG_CRITICAL, "Remove OpenCT and try again");
		return 1;
	}

	return 0;
} /* CheckForOpenCT */

/**
 * return the difference (as long int) in µs between 2 struct timeval
 * r = a - b
 */
long int time_sub(struct timeval *a, struct timeval *b)
{
	struct timeval r;
	r.tv_sec = a -> tv_sec - b -> tv_sec;
	r.tv_usec = a -> tv_usec - b -> tv_usec;
	if (r.tv_usec < 0)
	{
		r.tv_sec--;
		r.tv_usec += 1000000;
	}

	return r.tv_sec * 1000000 + r.tv_usec;
} /* time_sub */

int ThreadCreate(PCSCLITE_THREAD_T * pthThread, int attributes,
	PCSCLITE_THREAD_FUNCTION(pvFunction), LPVOID pvArg)
{
	pthread_attr_t attr;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret)
		return ret;

	ret = pthread_attr_setdetachstate(&attr,
		attributes & THREAD_ATTR_DETACHED ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
	if (ret)
	{
		(void)pthread_attr_destroy(&attr);
		return ret;
	}

	ret = pthread_create(pthThread, &attr, pvFunction, pvArg);
	if (ret)
		return ret;

	ret = pthread_attr_destroy(&attr);
	return ret;
}
