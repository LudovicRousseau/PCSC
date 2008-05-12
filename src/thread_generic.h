/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This provides system specific thread calls.
 */

#ifndef __thread_generic_h__
#define __thread_generic_h__

#ifdef WIN32
#include <windows.h>
#include "PCSC.h"
#else
#include <pthread.h>
#include <wintypes.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WIN32
#define PCSCLITE_THREAD_T                HANDLE
#define PCSCLITE_MUTEX                   CRITICAL_SECTION
#define PCSCLITE_MUTEX_T                 CRITICAL_SECTION*
#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)
#else
#define PCSCLITE_THREAD_T                pthread_t
#define PCSCLITE_MUTEX                   pthread_mutex_t
#define PCSCLITE_MUTEX_T                 pthread_mutex_t*
#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)
#endif

/* thread attributes */
#define THREAD_ATTR_DEFAULT			0
#define THREAD_ATTR_DETACHED		1

	int SYS_MutexInit(PCSCLITE_MUTEX_T);
	int SYS_MutexDestroy(PCSCLITE_MUTEX_T);
	int SYS_MutexLock(PCSCLITE_MUTEX_T);
	int SYS_MutexTryLock(PCSCLITE_MUTEX_T);
	int SYS_MutexUnLock(PCSCLITE_MUTEX_T);
	int SYS_ThreadCreate(PCSCLITE_THREAD_T *, int, PCSCLITE_THREAD_FUNCTION( ), LPVOID);
	int SYS_ThreadCancel(PCSCLITE_THREAD_T);
	int SYS_ThreadDetach(PCSCLITE_THREAD_T);
	int SYS_ThreadJoin(PCSCLITE_THREAD_T, LPVOID*);
	int SYS_ThreadExit(LPVOID);
	PCSCLITE_THREAD_T SYS_ThreadSelf(void);
	int SYS_ThreadEqual(PCSCLITE_THREAD_T *, PCSCLITE_THREAD_T *);
	int SYS_ThreadSetCancelType(int, int *);

#ifdef __cplusplus
}
#endif

#endif							/* __thread_generic_h__ */
