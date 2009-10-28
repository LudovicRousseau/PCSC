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

#include <pthread.h>
#include <wintypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PCSCLITE_THREAD_T                pthread_t
#define PCSCLITE_MUTEX                   pthread_mutex_t
#define PCSCLITE_THREAD_FUNCTION(f)      void *(*f)(void *)

/* thread attributes */
#define THREAD_ATTR_DEFAULT			0
#define THREAD_ATTR_DETACHED		1

	int SYS_MutexInit(PCSCLITE_MUTEX *);
	int SYS_MutexDestroy(PCSCLITE_MUTEX *);
	int SYS_MutexLock(PCSCLITE_MUTEX *);
	int SYS_MutexTryLock(PCSCLITE_MUTEX *);
	int SYS_MutexUnLock(PCSCLITE_MUTEX *);
	int SYS_ThreadCreate(PCSCLITE_THREAD_T *, int, PCSCLITE_THREAD_FUNCTION( ),
		/*@null@*/ LPVOID);
	int SYS_ThreadCancel(PCSCLITE_THREAD_T);
	int SYS_ThreadDetach(PCSCLITE_THREAD_T);
	int SYS_ThreadJoin(PCSCLITE_THREAD_T, /*@null@*/ LPVOID*);
	int SYS_ThreadExit(/*@null@*/ LPVOID);
	PCSCLITE_THREAD_T SYS_ThreadSelf(void);
	int SYS_ThreadEqual(PCSCLITE_THREAD_T *, PCSCLITE_THREAD_T *);
	int SYS_ThreadSetCancelType(int, int *);

#ifdef __cplusplus
}
#endif

#endif							/* __thread_generic_h__ */
