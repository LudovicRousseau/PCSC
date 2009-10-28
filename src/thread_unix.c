/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2000-2008
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles thread function abstraction.
 */

#include "config.h"
#include "wintypes.h"
#include "thread_generic.h"
#include "misc.h"

INTERNAL int SYS_MutexInit(PCSCLITE_MUTEX * mMutex)
{
	if (mMutex)
		return pthread_mutex_init(mMutex, NULL);
	else
		return -1;
}

INTERNAL int SYS_MutexDestroy(PCSCLITE_MUTEX * mMutex)
{
	if (mMutex)
		return pthread_mutex_destroy(mMutex);
	else
		return -1;
}

INTERNAL int SYS_MutexLock(PCSCLITE_MUTEX * mMutex)
{
	if (mMutex)
		return pthread_mutex_lock(mMutex);
	else
		return -1;
}

INTERNAL int SYS_MutexTryLock(PCSCLITE_MUTEX * mMutex)
{
	if (mMutex)
		return pthread_mutex_trylock(mMutex);
	else
		return -1;
}

INTERNAL int SYS_MutexUnLock(PCSCLITE_MUTEX * mMutex)
{
	if (mMutex)
		return pthread_mutex_unlock(mMutex);
	else
		return -1;
}

INTERNAL int SYS_ThreadCreate(PCSCLITE_THREAD_T * pthThread, int attributes,
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

INTERNAL int SYS_ThreadCancel(PCSCLITE_THREAD_T pthThread)
{
	return pthread_cancel(pthThread);
}

INTERNAL int SYS_ThreadDetach(PCSCLITE_THREAD_T pthThread)
{
	return pthread_detach(pthThread);
}

INTERNAL int SYS_ThreadJoin(PCSCLITE_THREAD_T pthThread, LPVOID* pvRetVal)
{
	return pthread_join(pthThread, pvRetVal);
}

INTERNAL int SYS_ThreadExit(LPVOID pvRetVal)
{
	pthread_exit(pvRetVal);
	return 1;
}

INTERNAL PCSCLITE_THREAD_T SYS_ThreadSelf(void)
{
	return pthread_self();
}

INTERNAL int SYS_ThreadEqual(PCSCLITE_THREAD_T *pthThread1, PCSCLITE_THREAD_T *pthThread2)
{
	return pthread_equal(*pthThread1, *pthThread2);
}

INTERNAL int SYS_ThreadSetCancelType(int type, int *oldtype)
{
	return pthread_setcanceltype(type, oldtype);
}

