/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : thread_generic.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 3/24/00
	    License: Copyright (C) 2000 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides system specific thread calls. 
	            
********************************************************************/

#ifndef __thread_generic_h__
#define __thread_generic_h__

#include <pthread.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PCSCLITE_THREAD_T                pthread_t
#define PCSCLITE_MUTEX                   pthread_mutex_t
#define PCSCLITE_MUTEX_T                 pthread_mutex_t*

	int SYS_MutexInit(PCSCLITE_MUTEX_T);

	int SYS_MutexDestroy(PCSCLITE_MUTEX_T);

	int SYS_MutexLock(PCSCLITE_MUTEX_T);

	int SYS_MutexUnLock(PCSCLITE_MUTEX_T);

	int SYS_ThreadCreate(PCSCLITE_THREAD_T *, LPVOID, LPVOID, LPVOID);

	int SYS_ThreadCancel(PCSCLITE_THREAD_T *);

	int SYS_ThreadDetach(PCSCLITE_THREAD_T);

        int SYS_ThreadJoin(PCSCLITE_THREAD_T *, LPVOID*);

	int SYS_ThreadExit(LPVOID);

#ifdef __cplusplus
}
#endif

#endif							/* __thread_generic_h__ */
