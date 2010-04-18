/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2003-2004
 *  Damien Sauveron <damien.sauveron@labri.fr>
 * Copyright (C) 2002-2009
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

