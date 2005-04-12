/*
 * This handles abstract system level calls.
 *
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

#include "config.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "sys_generic.h"
#include "debuglog.h"
#include "misc.h"

/**
 * @brief Make system wide initialization.
 *
 * @return Eror code.
 * @retval 0 Success.
 */
INTERNAL int SYS_Initialize(void)
{
	/*
	 * Nothing special
	 */
	return 0;
}

/**
 * @brief Attempts to create a directory with some permissions.
 *
 * @param[in] path Path of the directory to be created.
 * @param[in] perms Permissions to the new directory.
 *
 * @return Eror code.
 * @retval 0 Success.
 * @retval -1 An error occurred.
 */
INTERNAL int SYS_Mkdir(char *path, int perms)
{
	return mkdir(path, perms);
}

/**
 * @brief Gets the running process's ID.
 *
 * @return PID.
 */
INTERNAL int SYS_GetPID(void)
{
	return getpid();
}

/**
 * @brief Makes the current process sleep for some seconds.
 *
 * @param[in] iTimeVal Number of seconds to sleep.
 */
INTERNAL int SYS_Sleep(int iTimeVal)
{
#ifdef HAVE_NANOSLEEP
	struct timespec mrqtp;
	mrqtp.tv_sec = iTimeVal;
	mrqtp.tv_nsec = 0;

	return nanosleep(&mrqtp, NULL);
#else
	return sleep(iTimeVal);
#endif
}

/**
 * @brief Makes the current process sleep for some microseconds.
 *
 * @param[in] iTimeVal Number of microseconds to sleep.
 */
INTERNAL int SYS_USleep(int iTimeVal)
{
#ifdef HAVE_NANOSLEEP
	struct timespec mrqtp;
	mrqtp.tv_sec = 0;
	mrqtp.tv_nsec = iTimeVal * 1000;

	return nanosleep(&mrqtp, NULL);
#else
	usleep(iTimeVal);
	return iTimeVal;
#endif
}

/**
 * @brief Opens/creates a file.
 *
 * @param[in] pcFile path to the file.
 * @param[in] flags Open and read/write choices.
 * @param[in] mode Permissions to the file.
 *
 * @return File descriptor.
 * @retval >0 The file descriptor.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_OpenFile(char *pcFile, int flags, int mode)
{
	return open(pcFile, flags, mode);
}

/**
 * @brief Opens/creates a file.
 *
 * @param[in] iHandle File descriptor.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_CloseFile(int iHandle)
{
	return close(iHandle);
}

/**
 * @brief Removes a file.
 *
 * @param[in] pcFile path to the file.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_RemoveFile(char *pcFile)
{
	return remove(pcFile);
}

INTERNAL int SYS_Chmod(const char *path, int mode)
{
	return chmod(path, mode);
}

INTERNAL int SYS_Chdir(const char *path)
{
	return chdir(path);
}

INTERNAL int SYS_Mkfifo(const char *path, int mode)
{
	return mkfifo(path, mode);
}

INTERNAL int SYS_Mknod(const char *path, int mode, int dev)
{
	return mknod(path, mode, dev);
}

INTERNAL int SYS_GetUID(void)
{
	return getuid();
}

INTERNAL int SYS_GetGID(void)
{
	return getgid();
}

INTERNAL int SYS_Chown(const char *fname, int uid, int gid)
{
	return chown(fname, uid, gid);
}

INTERNAL int SYS_ChangePermissions(char *pcFile, int mode)
{
	return chmod(pcFile, mode);
}

/**
 * @brief Makes a non-blocking request to lock a file exclusively.
 *
 * @param[in] iHandle File descriptor.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_LockFile(int iHandle)
{
#ifdef HAVE_FLOCK
	return flock(iHandle, LOCK_EX | LOCK_NB);
#else
	struct flock lock_s;

	lock_s.l_type = F_WRLCK;
	lock_s.l_whence = 0;
	lock_s.l_start = 0L;
	lock_s.l_len = 0L;

	return fcntl(iHandle, F_SETLK, &lock_s);
#endif
}

/**
 * @brief Makes a blocking request to lock a file exclusively.
 *
 * @param[in] iHandle File descriptor.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_LockAndBlock(int iHandle)
{
#ifdef HAVE_FLOCK
	return flock(iHandle, LOCK_EX);
#else
	struct flock lock_s;

	lock_s.l_type = F_RDLCK;
	lock_s.l_whence = 0;
	lock_s.l_start = 0L;
	lock_s.l_len = 0L;

	return fcntl(iHandle, F_SETLKW, &lock_s);
#endif
}

/**
 * @brief Unlocks the file.
 *
 * @param[in] iHandle File descriptor.
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_UnlockFile(int iHandle)
{
#ifdef HAVE_FLOCK
	return flock(iHandle, LOCK_UN);
#else
	struct flock lock_s;

	lock_s.l_type = F_UNLCK;
	lock_s.l_whence = 0;
	lock_s.l_start = 0L;
	lock_s.l_len = 0L;

	return fcntl(iHandle, F_SETLK, &lock_s);
#endif
}

INTERNAL int SYS_SeekFile(int iHandle, int iSeekLength)
{
	int iOffset;
	iOffset = lseek(iHandle, iSeekLength, SEEK_SET);
	return iOffset;
}

INTERNAL int SYS_ReadFile(int iHandle, char *pcBuffer, int iLength)
{
	return read(iHandle, pcBuffer, iLength);
}

INTERNAL int SYS_WriteFile(int iHandle, char *pcBuffer, int iLength)
{
	return write(iHandle, pcBuffer, iLength);
}

/**
 * @brief Gets the memory page size.
 *
 * The page size is used when calling the \c SYS_MemoryMap() and 
 * \c SYS_PublicMemoryMap() functions.
 *
 * @return Number of bytes per page.
 */
INTERNAL int SYS_GetPageSize(void)
{
	return getpagesize();
}

/**
 * @brief Map the file \p iFid in memory for reading and writing.
 *
 * @param[in] iSize Size of the memmory mapped.
 * @param[in] iFid File which will be mapped in memory.
 * @param[in] iOffset Start point of the file to be mapped in memory.
 *
 * @return Address of the memory map.
 */
INTERNAL void *SYS_MemoryMap(int iSize, int iFid, int iOffset)
{

	void *vAddress;

	vAddress = 0;
	vAddress = mmap(0, iSize, PROT_READ | PROT_WRITE,
		MAP_SHARED, iFid, iOffset);

	/*
	 * Here are some common error types: switch( errno ) { case EINVAL:
	 * printf("EINVAL"); case EBADF: printf("EBADF"); break; case EACCES:
	 * printf("EACCES"); break; case EAGAIN: printf("EAGAIN"); break; case 
	 * ENOMEM: printf("ENOMEM"); break; } 
	 */

	return vAddress;
}

/**
 * @brief Map the file \p iFid in memory only for reading.
 *
 * @param[in] iSize Size of the memmory mapped.
 * @param[in] iFid File which will be mapped in memory.
 * @param[in] iOffset Start point of the file to be mapped in memory.
 *
 * @return Address of the memory map.
 */
INTERNAL void *SYS_PublicMemoryMap(int iSize, int iFid, int iOffset)
{

	void *vAddress;

	vAddress = 0;
	vAddress = mmap(0, iSize, PROT_READ, MAP_SHARED, iFid, iOffset);
	return vAddress;
}

/**
 * @brief Writes the changes made in a memory map to the disk mapped file.
 *
 * @param[in] begin Start of the block to be written
 * @param[in] lenght Lenght of the block to be written
 *
 * @return Error code.
 * @retval 0 Success.
 * @retval -1 An error ocurred.
 */
INTERNAL int SYS_MMapSynchronize(void *begin, int length)
{
	int flags = 0;

#ifdef MS_INVALIDATE
	flags |= MS_INVALIDATE;
#endif
	return msync(begin, length, MS_SYNC | flags);
}

INTERNAL int SYS_Fork(void)
{
	return fork();
}

/**
 * @brief put the process to run in the background.
 *
 * @param[in] nochdir if zero, change the current directory to "/".
 * @param[in] noclose if zero, redirect standard imput/output/error to /dev/nulll.
 *
 * @return error code.
 * @retval 0 success.
 * @retval -1 an error ocurred.
 */
INTERNAL int SYS_Daemon(int nochdir, int noclose)
{
#ifdef HAVE_DAEMON
	return daemon(nochdir, noclose);
#else
	switch (SYS_Fork())
	{
	case -1:
		return (-1);
	case 0:
		break;
	default:
		return (0);
	}

	if (!noclose) {
		if (SYS_CloseFile(0))
			Log2("SYS_CloseFile(0) failed: %s", strerror(errno));

		if (SYS_CloseFile(1))
			Log2("SYS_CloseFile(1) failed: %s", strerror(errno));

		if (SYS_CloseFile(2))
			Log2("SYS_CloseFile(2) failed: %s", strerror(errno));
	}
	if (!nochdir) {
		if (SYS_Chdir("/"))
			Log2("SYS_Chdir() failed: %s", strerror(errno));
	}
	return 0;
#endif
}

INTERNAL int SYS_Wait(int iPid, int iWait)
{
	return waitpid(-1, 0, WNOHANG);
}

INTERNAL int SYS_Stat(char *pcFile, struct stat *psStatus)
{
	return stat(pcFile, psStatus);
}

INTERNAL int SYS_Fstat(int iFd)
{
	struct stat sStatus;
	return fstat(iFd, &sStatus);
}

INTERNAL int SYS_RandomInt(int fStart, int fEnd)
{
	static int iInitialized = 0;
	int iRandNum = 0;

	if (0 == iInitialized)
	{
		srand(SYS_GetSeed());
		iInitialized = 1;
	}

	iRandNum = (int)((float)rand()/RAND_MAX * (fEnd - fStart)) + fStart;

	return iRandNum;
}

INTERNAL int SYS_GetSeed(void)
{
	struct timeval tv;
	struct timezone tz;
	long myseed = 0;

	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	if (gettimeofday(&tv, &tz) == 0)
	{
		myseed = tv.tv_usec;
	} else
	{
		myseed = (long) time(NULL);
	}
	return myseed;
}

INTERNAL void SYS_Exit(int iRetVal)
{
	_exit(iRetVal);
}

INTERNAL int SYS_Rmdir(char *pcFile)
{
	return rmdir(pcFile);
}

INTERNAL int SYS_Unlink(char *pcFile)
{
	return unlink(pcFile);
}

