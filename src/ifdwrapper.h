/*
 * MUSCLE SmartCard Development ( https://pcsclite.apdu.fr/ )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@musclecard.com>
 * Copyright (C) 2002-2021
 *  Ludovic Rousseau <ludovic.rouseau@free.fr>
 *
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * @brief This wraps the dynamic ifdhandler functions. The abstraction will
 * eventually allow multiple card slots in the same terminal.
 */

#ifndef __ifdwrapper_h__
#define __ifdwrapper_h__

#include "ifdhandler.h"
#include "readerfactory.h"
#include "wintypes.h"

	RESPONSECODE IFDOpenIFD(READER_CONTEXT *);
	RESPONSECODE IFDCloseIFD(READER_CONTEXT *);
	RESPONSECODE IFDPowerICC(READER_CONTEXT *, DWORD, PUCHAR, /*@out@*/ PDWORD);
	LONG IFDStatusICC(READER_CONTEXT *, /*@out@*/ PDWORD);
	LONG IFDControl_v2(READER_CONTEXT *, PUCHAR, DWORD, /*@out@*/ PUCHAR,
		PDWORD);
	LONG IFDControl(READER_CONTEXT *, DWORD, LPCVOID, DWORD, LPVOID,
		DWORD, LPDWORD);
	LONG IFDTransmit(READER_CONTEXT *, SCARD_IO_HEADER,
		PUCHAR, DWORD, /*@out@*/ PUCHAR, PDWORD, PSCARD_IO_HEADER);
	RESPONSECODE IFDSetPTS(READER_CONTEXT *, DWORD, UCHAR, UCHAR, UCHAR, UCHAR);
	RESPONSECODE IFDSetCapabilities(READER_CONTEXT *, DWORD, DWORD, PUCHAR);
	RESPONSECODE IFDGetCapabilities(READER_CONTEXT *, DWORD, PDWORD, /*@out@*/ PUCHAR);

#endif							/* __ifdwrapper_h__ */
