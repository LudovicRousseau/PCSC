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

#ifdef WIN32
#include <windows.h>
#include "PCSC.h"

#else
#include <pthread.h>
#define PCSC_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef WIN32
#define PCSCLITE_THREAD_T                HANDLE
#define PCSCLITE_MUTEX                   CRITICAL_SECTION
#define PCSCLITE_MUTEX_T                 CRITICAL_SECTION*

#else
#define PCSCLITE_THREAD_T                pthread_t
#define PCSCLITE_MUTEX                   pthread_mutex_t
#define PCSCLITE_MUTEX_T                 pthread_mutex_t*
#endif

	int SYS_MutexInit(PCSCLITE_MUTEX_T);
	int SYS_MutexDestroy(PCSCLITE_MUTEX_T);
	int SYS_MutexLock(PCSCLITE_MUTEX_T);
	int SYS_MutexUnLock(PCSCLITE_MUTEX_T);
	int SYS_ThreadCreate(PCSCLITE_THREAD_T *, LPVOID, LPVOID, LPVOID);
	int SYS_ThreadCancel(PCSCLITE_THREAD_T *);
	int SYS_ThreadDetach(PCSCLITE_THREAD_T);
        int SYS_ThreadJoin(PCSCLITE_THREAD_T *, LPVOID*);
	int SYS_ThreadExit(LPVOID);


        PCSC_API int MSC_MutexInit(PCSCLITE_MUTEX_T);
	PCSC_API int MSC_MutexDestroy(PCSCLITE_MUTEX_T);
	PCSC_API int MSC_MutexLock(PCSCLITE_MUTEX_T);
        PCSC_API int MSC_MutexUnLock(PCSCLITE_MUTEX_T);

#ifdef __cplusplus
}
#endif

#endif							/* __thread_generic_h__ */
