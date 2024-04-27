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

#ifndef __utils_h__
#define __utils_h__

#include <sys/types.h>
#include "wintypes.h"
#include "readerfactory.h"

long int time_sub(struct timeval *a, struct timeval *b);

/* defined in winscard_clnt.c */
LONG SCardCheckDaemonAvailability(void);

#ifndef LIBPCSCLITE

#define PID_ASCII_SIZE 11
pid_t GetDaemonPid(void);
int SendHotplugSignal(void);

int CheckForOpenCT(void);

/* thread attributes */
#define THREAD_ATTR_DEFAULT			0
#define THREAD_ATTR_DETACHED		1

#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)

int ThreadCreate(pthread_t *, int, PCSCLITE_THREAD_FUNCTION( ),
	/*@null@*/ LPVOID);

#endif

#endif

