/*
 * This handles thread function abstraction.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

#include "config.h"
#include "PCSC/wintypes.h"
#include "thread_generic.h"

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int SYS_MutexInit(PCSCLITE_MUTEX_T mMutex)
{
	return pthread_mutex_init(mMutex, NULL);
}

int SYS_MutexDestroy(PCSCLITE_MUTEX_T mMutex)
{
	return pthread_mutex_destroy(mMutex);
}

int SYS_MutexLock(PCSCLITE_MUTEX_T mMutex)
{
	return pthread_mutex_lock(mMutex);
}

int SYS_MutexUnLock(PCSCLITE_MUTEX_T mMutex)
{
	return pthread_mutex_unlock(mMutex);
}

int SYS_ThreadCreate(PCSCLITE_THREAD_T * pthThread, int attributes,
	PCSCLITE_THREAD_FUNCTION(pvFunction), LPVOID pvArg)
{
	pthread_attr_t attr;
	
	if (0 != pthread_attr_init(&attr))
		return FALSE;
	
	if (0 != pthread_attr_setdetachstate(&attr,
		attributes & THREAD_ATTR_DETACHED ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE))
		return FALSE;
	
	if (0 == pthread_create(pthThread, &attr, pvFunction, pvArg))
		return TRUE;
	else
		return FALSE;
}

int SYS_ThreadCancel(PCSCLITE_THREAD_T * pthThread)
{
	if (0 == pthread_cancel(*pthThread))
		return TRUE;
	else
		return FALSE;
}

int SYS_ThreadDetach(PCSCLITE_THREAD_T pthThread)
{
	if (0 == pthread_detach(pthThread))
		return TRUE;
	else
		return FALSE;
}

int SYS_ThreadJoin(PCSCLITE_THREAD_T *pthThread, LPVOID* pvRetVal)
{
	if (0 == pthread_join(*pthThread, pvRetVal))
		return TRUE;
	else
		return FALSE;
}

int SYS_ThreadExit(LPVOID pvRetVal)
{
	pthread_exit(pvRetVal);
	return 1;
}

PCSCLITE_THREAD_T SYS_ThreadSelf(void)
{
	return pthread_self();
}

int SYS_ThreadEqual(PCSCLITE_THREAD_T *pthThread1, PCSCLITE_THREAD_T *pthThread2)
{
	return pthread_equal(*pthThread1, *pthThread2);
}

