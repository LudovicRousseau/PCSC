/******************************************************************

    MUSCLE SmartCard Development ( http://www.linuxnet.com )
    Title  : thread_win32.c
    Package: pcsc lite
    Author : Jamie Nicolson
    Date   : 02/20/2003
    License: Copyright (C) 2003 Netscape Communications Corporation
    Purpose: This handles thread and mutex functions for Windows.

********************************************************************/

#include "thread_generic.h"
#include <assert.h>

/**
 * Returns: 1 for success, 0 for error.
 */
int
SYS_MutexInit(CRITICAL_SECTION *mutex)
{
    InitializeCriticalSection(mutex);
    return 1;
}


/**
 * Returns: nonzero for success, 0 for failure.
 */
int
SYS_MutexDestroy(CRITICAL_SECTION *mutex)
{
    DeleteCriticalSection(mutex);
    return 1;
}

/**
 * Returns: nonzero for success, 0 for failure.
 */
int
SYS_MutexLock(CRITICAL_SECTION *mutex)
{
    EnterCriticalSection(mutex);
    return 1;
}

/**
 * Returns: nonzero for success, 0 for failure.
 */
int
SYS_MutexUnLock(CRITICAL_SECTION *mutex)
{
    LeaveCriticalSection(mutex);
    return 1;
}

/*
 * Our thread start routine has the prototype:
 *
 *     void *(*start_routine)(void *arg);
 *
 * which is different from the prototype for the start routine of a
 * Win32 thread:
 *
 *     DWORD WINAPI (*start_routine)(void *arg);
 *
 * I don't know what WINAPI means. Assuming we can ignore that, the
 * difference between the two prototypes is the return type.  On
 * 64-bit Windows, void * is 64-bit but DWORD is 32-bit.
 *
 * The current implementation should work on 32-bit Windows. If we
 * want the code to be portable, we need to define PCSCLITE_THREAD_T
 * as a structure:
 *
 *     typedef struct {
 *         HANDLE handle;
 *         void *(start_routine)(void *);
 *         void *arg;
 *         void *rv;
 *     } PCSCLITE_THREAD_T;
 *
 * and define a Win32 thread start routine wrapper like this:
 *
 *     static DWORD WINAPI Win32StartRoutine(void *arg)
 *     {
 *         PCSCLITE_THREAD_T *thread = (PCSCLITE_THREAD_T *)arg;
 *         thread->rv = thread->start_routine(thread->arg);
 *         return 0;
 *     }
 */

/**
 * Returns: nonzero for success, 0 for failure.
 */
int
SYS_ThreadCreate(HANDLE *thread, void* thread_attr,
    void* start_routine, void* arg)
{

    *thread = CreateThread(
                NULL,       /* thread is not inheritable by child processes */
                0,          /* default stack size */
                start_routine,
                arg,
                0,          /* no flags */
                NULL        /* don't receive thread ID */
              );

    if( *thread == NULL ) {
        return 0;
    } else {
        return 1;
    }
}

/**
 * Returns: nonzero for success, 0 for failure.
 */
int
SYS_ThreadJoin(HANDLE *thread, void **retval)
{
    DWORD status;
    BOOL rv;

    /* go to sleep waiting for the thread to exit */
    if( WaitForSingleObject(*thread, INFINITE) == WAIT_FAILED ) {
        return 0;
    }
    if( retval != NULL ) {
        rv = GetExitCodeThread(*thread, &status);
        if( rv == 0 ) {
            /* the call failed */
            return 0;
        }
        *retval = (void *)status;
    }
    /* success */
    CloseHandle(*thread);
    *thread = NULL;
    return 1;
}

int
SYS_ThreadExit(void* arg)
{
    DWORD status;

    status = (DWORD)arg;  /* FIXME: will truncate on 64-bit Windows */
    ExitThread(status);

    /* should not get here--we just exited the thread! */
    assert(0);

    return 0;
}

int MSC_MutexInit(PCSCLITE_MUTEX_T theMutex) {
  return SYS_MutexInit(theMutex);
}

int MSC_MutexDestroy(PCSCLITE_MUTEX_T theMutex) {
  return SYS_MutexDestroy(theMutex);
}

int MSC_MutexLock(PCSCLITE_MUTEX_T theMutex) {
  return SYS_MutexLock(theMutex);
}

int MSC_MutexUnLock(PCSCLITE_MUTEX_T theMutex) {
  return SYS_MutexUnLock(theMutex);
}