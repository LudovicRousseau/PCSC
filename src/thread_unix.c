/*
 * This handles thread function abstraction.
 *
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id$
 */

#include "config.h"
#include "wintypes.h"
#include "pcsclite.h"
#include "thread_generic.h"

#define PCSC_MUTEX_LOCKED    1
#define PCSC_MUTEX_UNLOCKED  0

int SYS_MutexInit(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_init(mMutex, NULL);
	return retval;
}

int SYS_MutexDestroy(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_destroy(mMutex);
	return retval;
}

int SYS_MutexLock(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_lock(mMutex);
	return retval;
}

int SYS_MutexUnLock(PCSCLITE_MUTEX_T mMutex)
{
	int retval;
	retval = pthread_mutex_unlock(mMutex);
	return retval;
}

int SYS_ThreadCreate(PCSCLITE_THREAD_T * pthThread, LPVOID pthAttr,
	LPVOID pvFunction, LPVOID pvArg)
{

	int retval;
	retval = pthread_create(pthThread, NULL, pvFunction, pvArg);

	if (retval == 0)
	{
		return 1;	/* TRUE */
	} else
	{
		return 0;	/* FALSE */
	}
}

int SYS_ThreadCancel(PCSCLITE_THREAD_T * pthThread)
{

	int retval;
	retval = pthread_cancel(*pthThread);

	if (retval == 0)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int SYS_ThreadDetach(PCSCLITE_THREAD_T pthThread)
{

	int retval;
	retval = pthread_detach(pthThread);

	if (retval == 0)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int SYS_ThreadJoin(PCSCLITE_THREAD_T *pthThread, LPVOID* pvRetVal)
{

	int retval;
	retval = pthread_join(*pthThread, pvRetVal);

	if (retval == 0)
	{
		return 1;
	} else
	{
		return 0;
	}
}

int SYS_ThreadExit(LPVOID pvRetVal)
{

	pthread_exit(pvRetVal);
	return 1;
}

PCSCLITE_THREAD_T SYS_ThreadSelf()
{
	return pthread_self();
}

int SYS_ThreadEqual(PCSCLITE_THREAD_T *pthThread1, PCSCLITE_THREAD_T *pthThread2)
{
	int retval;
	retval = pthread_equal(*pthThread1, *pthThread2);

	if (retval == 0)
	{
		return 0;
	} else
	{
		return 1;
	}
}
