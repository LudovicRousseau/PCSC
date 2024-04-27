/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 2006-2021
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

#ifndef LIBPCSCLITE
pid_t GetDaemonPid(void)
{
	int fd;
	pid_t pid;

	/* pids are only 15 bits but 4294967296
	 * (32 bits in case of a new system use it) is on 10 bytes
	 */
	fd = open(PCSCLITE_RUN_PID, O_RDONLY);
	if (fd >= 0)
	{
		char pid_ascii[PID_ASCII_SIZE];
		ssize_t r;

		r = read(fd, pid_ascii, sizeof pid_ascii);
		if (r < 0)
		{
			Log2(PCSC_LOG_CRITICAL, "Reading " PCSCLITE_RUN_PID " failed: %s",
				strerror(errno));
			pid = -1;
		}
		else
			pid = atoi(pid_ascii);
		(void)close(fd);

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
		Log2(PCSC_LOG_INFO, "Send hotplug signal to pcscd (pid=%ld)",
			(long)pid);
		if (kill(pid, SIGUSR1) < 0)
		{
			Log3(PCSC_LOG_CRITICAL, "Can't signal pcscd (pid=%ld): %s",
				(long)pid, strerror(errno));
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
		Log1(PCSC_LOG_CRITICAL, "File " OPENCT_FILE " found. Remove OpenCT and try again");
		return 1;
	}

	return 0;
} /* CheckForOpenCT */
#endif

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

#ifndef LIBPCSCLITE
int ThreadCreate(pthread_t * pthThread, int attributes,
	PCSCLITE_THREAD_FUNCTION(pvFunction), LPVOID pvArg)
{
	pthread_attr_t attr;
	size_t stack_size;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret)
		return ret;

	ret = pthread_attr_setdetachstate(&attr,
		attributes & THREAD_ATTR_DETACHED ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE);
	if (ret)
		goto error;

	/* stack size of 0x40000 (256 KB) bytes minimum for musl C lib */
	ret = pthread_attr_getstacksize(&attr, &stack_size);
	if (ret)
		goto error;

	/* A stack_size of 0 indicates the default size on Solaris.
	 * The default size is more than 256 KB so do not shrink it. */
	if ((stack_size != 0) && (stack_size < 0x40000))
	{
		stack_size = 0x40000;
		ret = pthread_attr_setstacksize(&attr, stack_size);
		if (ret)
			goto error;
	}

	ret = pthread_create(pthThread, &attr, pvFunction, pvArg);

error:
	pthread_attr_destroy(&attr);
	return ret;
}
#endif
