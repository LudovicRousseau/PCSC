/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2009
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This handles abstract system level calls.
 */

#ifndef __sys_generic_h__
#define __sys_generic_h__

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/stat.h>
#include <sys/mman.h>

	int SYS_Mkdir(const char *, int);

	int SYS_Sleep(int);

	int SYS_USleep(int);

	int SYS_OpenFile(const char *, int, int);

	int SYS_CloseFile(int);

	int SYS_RemoveFile(const char *);

	int SYS_Chmod(const char *, int);

	int SYS_Chdir(const char *);

	int SYS_WriteFile(int, const char *, int);

	int SYS_Daemon(int, int);

	int SYS_Stat(const char *pcFile, /*@out@*/ struct stat *psStatus);

	int SYS_RandomInt(int, int);

	int SYS_GetSeed(void);

	void SYS_Exit(int);

#ifdef __cplusplus
}
#endif

#endif							/* __sys_generic_h__ */
