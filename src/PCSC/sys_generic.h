
/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : sys_generic.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 11/8/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles abstract system level calls. 
	            
********************************************************************/

#ifndef __sys_generic_h__
#define __sys_generic_h__

#ifdef __cplusplus
extern "C" {
#endif     

#include <sys/stat.h>

  int SYS_Initialize();

  int SYS_Mkdir( char*, int );

  int SYS_GetPID( );

  int SYS_Sleep( int );
  
  int SYS_USleep( int );
    
  int SYS_OpenFile( char*, int, int );

  int SYS_CloseFile( int );

  int SYS_RemoveFile( char* );

  int SYS_Chmod( const char*, int );

  int SYS_Mkfifo( const char*, int );

  int SYS_Mknod( const char*, int, int );

  int SYS_GetUID();

  int SYS_GetGID();

  int SYS_Chown( const char*, int, int );

  int SYS_ChangePermissions( char*, int );

  int SYS_LockFile( int );

  int SYS_LockAndBlock( int );

  int SYS_UnlockFile( int );
  
  int SYS_SeekFile( int, int );
  
  int SYS_ReadFile( int, char*, int );
  
  int SYS_WriteFile( int, char*, int );
  
  int SYS_GetPageSize( void );
  
  void *SYS_MemoryMap( int, int, int );

  void *SYS_PublicMemoryMap( int, int, int );
  
  int SYS_MMapSynchronize( void*, int );

  int SYS_Fork();
  
  int SYS_Wait( int, int );
  
  int SYS_Stat( char *pcFile, struct stat *psStatus );

  int SYS_Fstat( int );
  
  int SYS_Random( int, float, float );

  int SYS_GetSeed();

  int SYS_Exit( int );

  int SYS_Rmdir( char *pcFile );

  int SYS_Unlink( char *pcFile );

#ifdef __cplusplus
}
#endif     

#endif /* __sys_generic_h__ */
