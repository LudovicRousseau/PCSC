/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : dyn_generic.h
	    Package: pcsc lite
            Author : David Corcoran
            Date   : 8/12/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This abstracts dynamic library loading 
                     functions. 

********************************************************************/

#ifndef __dyn_generic_h__
#define __dyn_generic_h__

#ifdef __cplusplus
extern "C"
{
#endif

	int DYN_LoadLibrary(void **, char *);
	int DYN_CloseLibrary(void **);
	int DYN_GetAddress(void *, void **, char *);

#ifdef __cplusplus
}
#endif

#endif
