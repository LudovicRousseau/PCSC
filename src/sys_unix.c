/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : sys_unix.c
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 11/8/99
	    License: Copyright (C) 1999 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This handles abstract system level calls. 
	            
********************************************************************/

#include <sys_generic.h>
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
#include <signal.h>
#include <time.h>

#include "config.h"

int SYS_Initialize() {
  /* Nothing special for OS X and Linux */
  return 0;
}

int SYS_Mkdir( char *path, int perms ) {
  return mkdir(path, perms);
}

int SYS_GetPID() {
  return getpid();
}

int SYS_Sleep( int iTimeVal ) {
  return sleep( iTimeVal );
}

int SYS_USleep( int iTimeVal ) {
  usleep( iTimeVal );
  return iTimeVal;
}

int SYS_OpenFile( char *pcFile, int flags, int mode ) {
  return open( pcFile, flags, mode );
}

int SYS_CloseFile( int iHandle ) {
  return close( iHandle );
}

int SYS_RemoveFile( char *pcFile ) {
  return remove( pcFile );
}

int SYS_Chmod( const char *path, int mode ) {
  return chmod( path, mode );
}

int SYS_Mkfifo( const char *path, int mode ) {
  return mkfifo( path, mode );
}

int SYS_Mknod( const char *path, int mode, int dev ) {
  return mknod( path, mode, dev );
}

int SYS_GetUID() {
  return getuid();
}

int SYS_GetGID() {
  return getgid();
}

int SYS_Chown( const char *fname, int uid, int gid ) {
  return chown( fname, uid, gid );
}

int SYS_ChangePermissions( char *pcFile, int mode ) {
  return chmod( pcFile, mode );
}

int SYS_LockFile( int iHandle ) {
  return flock( iHandle, LOCK_EX | LOCK_NB );
}

int SYS_LockAndBlock( int iHandle ) {
  return flock( iHandle, LOCK_EX );
}

int SYS_UnlockFile( int iHandle ) {
  return flock( iHandle, LOCK_UN );
}

int SYS_SeekFile( int iHandle, int iSeekLength ) {
  int iOffset;
  iOffset = lseek( iHandle, iSeekLength, SEEK_SET );
  return iOffset;
}

int SYS_ReadFile( int iHandle, char *pcBuffer, int iLength ) {
  return read( iHandle, pcBuffer, iLength );
}

int SYS_WriteFile( int iHandle, char *pcBuffer, int iLength ) {
  return write( iHandle, pcBuffer, iLength );
}

int SYS_GetPageSize( void ) {
  return getpagesize();
}

void *SYS_MemoryMap( int iSize, int iFid, int iOffset ) {

  void *vAddress;

  vAddress = 0;
  vAddress = mmap( 0, iSize, PROT_READ | PROT_WRITE,
		   MAP_SHARED, iFid, iOffset ); 

  /* Here are some common error types:
  switch( errno ) {
  case EINVAL:
    printf("EINVAL");
  case EBADF:
    printf("EBADF");
    break;
  case EACCES:
    printf("EACCES");
    break;
  case EAGAIN:
    printf("EAGAIN");
    break;
  case ENOMEM:
    printf("ENOMEM");
    break;
  }
  */

  return vAddress;
}

void *SYS_PublicMemoryMap( int iSize, int iFid, int iOffset ) {

  void *vAddress;

  vAddress = 0;
  vAddress = mmap( 0, iSize, PROT_READ,
		   MAP_SHARED, iFid, iOffset ); 
  return vAddress;
}

int SYS_MMapSynchronize( void *begin, int length ) {
  return msync(begin, length, MS_SYNC | MS_INVALIDATE);
}

int SYS_Fork() {
  return fork();
}

int SYS_Wait( int iPid, int iWait ) {
 return waitpid( -1, 0, WNOHANG );
}

int SYS_Stat( char *pcFile, struct stat *psStatus ) {
  return stat( pcFile, psStatus );
}

int SYS_Fstat( int iFd ) {
  struct stat sStatus;
  return fstat( iFd, &sStatus );
}

int SYS_Random( int iSeed, float fStart, float fEnd ) {

  int iRandNum = 0;

  if ( iSeed != 0 ) {
    srand( iSeed );
  }

  iRandNum = 1+(int)(fEnd*rand()/(RAND_MAX+fStart)); 
  srand( iRandNum );

  return iRandNum;
}

int SYS_GetSeed() {
  struct timeval tv;
  struct timezone tz;
  long myseed = 0;  

  tz.tz_minuteswest = 0;
  tz.tz_dsttime = 0;
  if (gettimeofday(&tv, &tz) == 0) {
    myseed = tv.tv_usec;
  } else {
    myseed = (long ) time(NULL);
  }                                   
  return myseed;
}

int SYS_Exit( int iRetVal ) {
  _exit( iRetVal );
}

int SYS_Rmdir( char *pcFile )
{
	return rmdir(pcFile);
}

int SYS_Unlink( char *pcFile )
{
	return unlink(pcFile);
}

