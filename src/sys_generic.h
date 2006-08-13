/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
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

	int SYS_Initialize(void);

	int SYS_Mkdir(char *, int);

	int SYS_GetPID(void);

	int SYS_Sleep(int);

	int SYS_USleep(int);

	int SYS_OpenFile(char *, int, int);

	int SYS_CloseFile(int);

	int SYS_RemoveFile(char *);

	int SYS_Chmod(const char *, int);

	int SYS_Chdir(const char *);

	int SYS_GetUID(void);

	int SYS_GetGID(void);

	int SYS_ChangePermissions(char *, int);

	int SYS_SeekFile(int, int);

	int SYS_ReadFile(int, char *, int);

	int SYS_WriteFile(int, char *, int);

	int SYS_GetPageSize(void);

	void *SYS_MemoryMap(int, int, int);

	void *SYS_PublicMemoryMap(int, int, int);

	void SYS_PublicMemoryUnmap(void *, int);

	int SYS_MMapSynchronize(void *, int);

	int SYS_Fork(void);

	int SYS_Daemon(int, int);

	int SYS_Stat(char *pcFile, struct stat *psStatus);

	int SYS_RandomInt(int, int);

	int SYS_GetSeed(void);

	void SYS_Exit(int);

	int SYS_Unlink(char *pcFile);

#ifdef __cplusplus
}
#endif

#endif							/* __sys_generic_h__ */
