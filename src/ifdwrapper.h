/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2002-2010
 *  Ludovic Rousseau <ludovic.rouseau@free.fr>
 *
 * $Id$
 */

/**
 * @file
 * @brief This wraps the dynamic ifdhandler functions. The abstraction will
 * eventually allow multiple card slots in the same terminal.
 */

#ifndef __ifdwrapper_h__
#define __ifdwrapper_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG IFDOpenIFD(READER_CONTEXT *);
	LONG IFDCloseIFD(READER_CONTEXT *);
	LONG IFDPowerICC(READER_CONTEXT *, DWORD, PUCHAR, /*@out@*/ PDWORD);
	LONG IFDStatusICC(READER_CONTEXT *, /*@out@*/ PDWORD, /*@out@*/ PUCHAR,
		/*@out@*/ PDWORD);
	LONG IFDControl_v2(READER_CONTEXT *, PUCHAR, DWORD, /*@out@*/ PUCHAR,
		PDWORD);
	LONG IFDControl(READER_CONTEXT *, DWORD, LPCVOID, DWORD, LPVOID,
		DWORD, LPDWORD);
	LONG IFDTransmit(READER_CONTEXT *, SCARD_IO_HEADER,
		PUCHAR, DWORD, /*@out@*/ PUCHAR, PDWORD, PSCARD_IO_HEADER);
	LONG IFDSetPTS(READER_CONTEXT *, DWORD, UCHAR, UCHAR, UCHAR, UCHAR);
	LONG IFDSetCapabilities(READER_CONTEXT *, DWORD, DWORD, PUCHAR);
	LONG IFDGetCapabilities(READER_CONTEXT *, DWORD, PDWORD, /*@out@*/ PUCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __ifdwrapper_h__ */
