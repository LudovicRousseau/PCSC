/******************************************************************

            Title  : prothandler.h
            Package: PC/SC Lite
            Author : David Corcoran
            Date   : 10/06/99
	    License: Copyright (C) 1999 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This handles protocol defaults, PTS, etc.

********************************************************************/

#ifndef __prothandler_h__
#define __prothandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

	UCHAR PHGetDefaultProtocol(PUCHAR, DWORD);
	UCHAR PHGetAvailableProtocols(PUCHAR, DWORD);
	UCHAR PHSetProtocol(struct ReaderContext *, DWORD, UCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __prothandler_h__ */
