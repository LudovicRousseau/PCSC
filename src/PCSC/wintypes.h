/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : wintypes.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 7/27/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This keeps a list of Windows(R) types.
	            
********************************************************************/

#ifndef __wintypes_h__
#define __wintypes_h__

#ifdef __cplusplus
extern "C" {
#endif  
  
#ifndef BYTE
  typedef unsigned char                 BYTE;
#endif
  typedef unsigned char                 UCHAR;
  typedef unsigned char*                PUCHAR;
  typedef unsigned short                USHORT;

#ifndef __COREFOUNDATION_CFPLUGINCOM__
  typedef unsigned long                 ULONG;
  typedef void *                        LPVOID;
  typedef short                         BOOL;
#endif

  typedef unsigned long*                PULONG;
  typedef const void *                  LPCVOID;
  typedef unsigned long                 DWORD;
  typedef unsigned long*                PDWORD;
  typedef DWORD                         WORD;
  typedef long                          LONG;
  typedef long                          RESPONSECODE;
  typedef const char *                  LPCSTR;
  typedef const BYTE *                  LPCBYTE;
  typedef BYTE *                        LPBYTE;
  typedef DWORD *                       LPDWORD;
  typedef char *                        LPSTR;
  typedef char *                        LPTSTR;
  typedef char *                        LPCWSTR;    

#ifdef __cplusplus
}
#endif 

#endif

